// Ver usb_direct.h.
#include "usb_direct.h"

#include <android/log.h>
#include <linux/usbdevice_fs.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "clock.h"

#define LOG_TAG "OrimbuxUsbAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace orimbux {

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr double kSr = 48000.0;
constexpr int kMaxLat = 64;

usbdevfs_urb* allocUrb(int packets) {
    const size_t size = sizeof(usbdevfs_urb) + static_cast<size_t>(packets) * sizeof(usbdevfs_iso_packet_desc);
    return static_cast<usbdevfs_urb*>(std::calloc(1, size));
}

// Prepara (ou volta a preparar) um URB isócrono com N pacotes de `packetBytes`.
void setupIsoUrb(usbdevfs_urb* u, int endpoint, void* buffer, int packets, int packetBytes, int index) {
    u->type = USBDEVFS_URB_TYPE_ISO;
    u->endpoint = static_cast<unsigned char>(endpoint);
    u->status = 0;
    u->flags = USBDEVFS_URB_ISO_ASAP;  // o controlador agenda logo a seguir ao anterior
    u->buffer = buffer;
    u->buffer_length = packets * packetBytes;
    u->actual_length = 0;
    u->start_frame = 0;
    u->number_of_packets = packets;
    u->error_count = 0;
    u->signr = 0;
    u->usercontext = reinterpret_cast<void*>(static_cast<intptr_t>(index));
    for (int p = 0; p < packets; ++p) {
        u->iso_frame_desc[p].length = static_cast<unsigned int>(packetBytes);
        u->iso_frame_desc[p].actual_length = 0;
        u->iso_frame_desc[p].status = 0;
    }
}

std::string errText(const char* what, int e) {
    return std::string(what) + " falhou: " + std::strerror(e) + " (errno " + std::to_string(e) + ")";
}

// Cancela os URBs em voo e espera que voltem (até ~300 ms).
void discardAndDrain(int fd, std::vector<void*>& urbs, int inFlight) {
    for (void* u : urbs) ioctl(fd, USBDEVFS_DISCARDURB, u);  // EINVAL se já não estava em voo: ignora
    const int64_t deadline = monoNs() + 300'000'000LL;
    while (inFlight > 0 && monoNs() < deadline) {
        usbdevfs_urb* done = nullptr;
        if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &done) == 0) {
            inFlight--;
        } else if (errno == EAGAIN) {
            pollfd p{fd, POLLOUT, 0};
            poll(&p, 1, 10);
        } else {
            break;  // ENODEV: o comando saiu
        }
    }
}
} // namespace

DirectUsbOut& DirectUsbOut::get() {
    static DirectUsbOut instance;
    return instance;
}

std::string DirectUsbOut::start(int fd, int endpoint, int numUrbs, int packetsPerUrb) {
    stop();
    fd_ = fd;
    endpoint_ = endpoint;
    numUrbs_ = std::clamp(numUrbs, 2, 16);
    packetsPerUrb_ = std::clamp(packetsPerUrb, 1, 8);
    const int frames = kFramesPerPacket * packetsPerUrb_;
    const int packetBytes = kFramesPerPacket * kChannels * 2;  // 384

    buffers_.assign(static_cast<size_t>(numUrbs_), std::vector<int16_t>(static_cast<size_t>(frames * kChannels)));
    urbTriggerT0_.assign(static_cast<size_t>(numUrbs_), 0);
    urbs_.clear();
    tonePhase_ = pulsePhase_ = clickPhase_ = burstPhase_ = 0;
    toneGain_ = 0;
    frameCounter_ = 0;
    clickPos_ = burstPos_ = -1;
    lastSeq_ = triggerSeq_.load();
    clearStats();

    // Enche e submete todos os URBs antes de arrancar a thread.
    int submitted = 0;
    for (int i = 0; i < numUrbs_; ++i) {
        usbdevfs_urb* u = allocUrb(packetsPerUrb_);
        urbs_.push_back(u);
        fill(buffers_[i].data(), frames, i);
        setupIsoUrb(u, endpoint_, buffers_[i].data(), packetsPerUrb_, packetBytes, i);
        if (ioctl(fd_, USBDEVFS_SUBMITURB, u) < 0) {
            const int e = errno;
            discardAndDrain(fd_, urbs_, submitted);
            for (void* p : urbs_) std::free(p);
            urbs_.clear();
            LOGW("SUBMITURB inicial falhou: %s", std::strerror(e));
            std::lock_guard<std::mutex> lock(statsMutex_);
            st_.lastErrno = e;
            return errText("USBDEVFS_SUBMITURB", e);
        }
        submitted++;
    }
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        st_.queueMs = static_cast<float>(numUrbs_ * packetsPerUrb_);
    }
    startNs_ = monoNs();
    lastReapNs_ = 0;
    running_ = true;
    thread_ = std::thread([this] { loop(); });
    LOGI("Áudio USB direto: ep=0x%02x, %d URBs × %d pacotes (fila de %d ms)", endpoint_, numUrbs_, packetsPerUrb_,
         numUrbs_ * packetsPerUrb_);
    return "";
}

void DirectUsbOut::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void DirectUsbOut::padPressed(int64_t t0MonoNs) {
    if (!running_) return;
    triggerT0_.store(t0MonoNs, std::memory_order_relaxed);
    triggerSeq_.fetch_add(1, std::memory_order_release);
}

// Gera `frames` frames de 4 canais (16 bits) para o URB `urbIndex`.
void DirectUsbOut::fill(int16_t* dst, int frames, int urbIndex) {
    const uint32_t seq = triggerSeq_.load(std::memory_order_acquire);
    if (seq != lastSeq_) {
        // Pad carregado: o som começa no primeiro frame deste URB.
        lastSeq_ = seq;
        urbTriggerT0_[static_cast<size_t>(urbIndex)] = triggerT0_.load(std::memory_order_relaxed);
        clickPos_ = 0;
        clickPhase_ = 0;
        burstPos_ = 0;
        burstPhase_ = 0;
    }
    const float target = tone_ ? jackLevel_.load() : 0.0f;
    const float rampStep = 1.0f / 240.0f / 4.0f;  // ~5 ms até ao nível 0,25 (ligar/desligar sem estalido)
    const int64_t pulsePeriod = 48000;     // 1 s
    const int64_t pulseLen = 5760;         // 120 ms
    const int burstLen = 2880;             // 60 ms
    const int clickLen = 1440;             // 30 ms

    for (int f = 0; f < frames; ++f) {
        float jack = 0.0f;
        float hap = 0.0f;

        // Tom contínuo de 440 Hz no jack
        if (toneGain_ < target) toneGain_ = std::min(target, toneGain_ + rampStep);
        else if (toneGain_ > target) toneGain_ = std::max(target, toneGain_ - rampStep);
        if (toneGain_ > 0.0f) {
            jack += static_cast<float>(std::sin(tonePhase_)) * toneGain_;
            tonePhase_ += kTwoPi * 440.0 / kSr;
            if (tonePhase_ > kTwoPi) tonePhase_ -= kTwoPi;
        }

        // Pulso háptico de 80 Hz: 120 ms a cada segundo
        if (pulse_) {
            const int64_t pos = frameCounter_ % pulsePeriod;
            if (pos < pulseLen) {
                const float env = std::min({1.0f, static_cast<float>(pos) / 144.0f,
                                            static_cast<float>(pulseLen - pos) / 144.0f});
                hap += static_cast<float>(std::sin(pulsePhase_)) * 0.9f * env;
                pulsePhase_ += kTwoPi * 80.0 / kSr;
                if (pulsePhase_ > kTwoPi) pulsePhase_ -= kTwoPi;
            } else {
                pulsePhase_ = 0;
            }
        }

        // Pad carregado: clique de 1,5 kHz no jack + impulso de 80 Hz na háptica
        if (clickPos_ >= 0) {
            const double t = clickPos_ / kSr;
            jack += static_cast<float>(std::sin(clickPhase_) * std::exp(-t / 0.008)) * 0.8f;
            clickPhase_ += kTwoPi * 1500.0 / kSr;
            if (++clickPos_ >= clickLen) clickPos_ = -1;
        }
        if (burstPos_ >= 0) {
            const float env = std::min({1.0f, static_cast<float>(burstPos_) / 144.0f,
                                        static_cast<float>(burstLen - burstPos_) / 144.0f});
            hap += static_cast<float>(std::sin(burstPhase_)) * env;
            burstPhase_ += kTwoPi * 80.0 / kSr;
            if (++burstPos_ >= burstLen) burstPos_ = -1;
        }

        const auto j = static_cast<int16_t>(std::clamp(jack, -1.0f, 1.0f) * 32767.0f);
        const auto h = static_cast<int16_t>(std::clamp(hap, -1.0f, 1.0f) * 32767.0f);
        int16_t* out = dst + f * kChannels;
        out[0] = j;  // canal 1: jack L
        out[1] = j;  // canal 2: jack R
        out[2] = h;  // canal 3: háptica L
        out[3] = h;  // canal 4: háptica R
        frameCounter_++;
    }
}

void DirectUsbOut::loop() {
    setpriority(PRIO_PROCESS, static_cast<id_t>(gettid()), -19);  // prioridade de áudio (como o Android faz)
    const int frames = kFramesPerPacket * packetsPerUrb_;
    const int packetBytes = kFramesPerPacket * kChannels * 2;
    const int64_t urbNs = static_cast<int64_t>(packetsPerUrb_) * 1'000'000LL;
    int inFlight = numUrbs_;

    while (running_) {
        pollfd p{fd_, POLLOUT, 0};
        const int pr = poll(&p, 1, 100);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pr == 0) continue;
        if (p.revents & (POLLERR | POLLHUP)) {
            LOGW("poll: comando desligado");
            running_ = false;
            break;
        }
        // Recolhe todos os URBs concluídos e volta a enchê-los.
        while (running_) {
            usbdevfs_urb* done = nullptr;
            if (ioctl(fd_, USBDEVFS_REAPURBNDELAY, &done) < 0) {
                if (errno == EAGAIN) break;
                const int e = errno;
                LOGW("REAPURB falhou: %s", std::strerror(e));
                std::lock_guard<std::mutex> lock(statsMutex_);
                st_.lastErrno = e;
                if (e == ENODEV) running_ = false;
                break;
            }
            inFlight--;
            const int64_t now = monoNs();
            const int idx = static_cast<int>(reinterpret_cast<intptr_t>(done->usercontext));
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                st_.urbsDone++;
                for (int k = 0; k < done->number_of_packets; ++k) {
                    if (done->iso_frame_desc[k].status != 0) st_.packetErrors++;
                }
                if (lastReapNs_ != 0) {
                    st_.maxGapMs = std::max(st_.maxGapMs, static_cast<float>(now - lastReapNs_) / 1e6f);
                }
                // Fila vazia = o controlador ficou sem dados = corte audível.
                if (inFlight == 0) st_.underruns++;
                const int64_t t0 = urbTriggerT0_[static_cast<size_t>(idx)];
                if (t0 != 0) {
                    // O 1.º pacote deste URB saiu ~(pacotes−1) ms antes de o URB terminar.
                    const int64_t firstPacketNs = now - urbNs + 1'000'000LL;
                    const float lat = static_cast<float>(firstPacketNs - t0) / 1e6f;
                    if (lat > 0 && lat < 1000) {
                        if (lat_.size() >= kMaxLat) lat_.erase(lat_.begin());
                        lat_.push_back(lat);
                    }
                    urbTriggerT0_[static_cast<size_t>(idx)] = 0;
                }
            }
            lastReapNs_ = now;

            fill(buffers_[static_cast<size_t>(idx)].data(), frames, idx);
            setupIsoUrb(done, endpoint_, buffers_[static_cast<size_t>(idx)].data(), packetsPerUrb_, packetBytes, idx);
            if (ioctl(fd_, USBDEVFS_SUBMITURB, done) < 0) {
                const int e = errno;
                std::lock_guard<std::mutex> lock(statsMutex_);
                st_.submitErrors++;
                st_.lastErrno = e;
                if (e == ENODEV) running_ = false;
            } else {
                inFlight++;
            }
        }
    }

    discardAndDrain(fd_, urbs_, inFlight);
    for (void* u : urbs_) std::free(u);
    urbs_.clear();
    running_ = false;
    LOGI("Áudio USB direto parado");
}

DirectStats DirectUsbOut::stats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    DirectStats s = st_;
    s.running = running_ ? 1 : 0;
    s.elapsedS = running_ ? static_cast<float>(monoNs() - startNs_) / 1e9f : st_.elapsedS;
    s.latCount = static_cast<int>(lat_.size());
    if (!lat_.empty()) {
        s.latMin = *std::min_element(lat_.begin(), lat_.end());
        s.latMax = *std::max_element(lat_.begin(), lat_.end());
        float sum = 0;
        for (float v : lat_) sum += v;
        s.latAvg = sum / static_cast<float>(lat_.size());
        s.latLast = lat_.back();
    }
    st_.elapsedS = s.elapsedS;
    return s;
}

void DirectUsbOut::clearStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    const float q = st_.queueMs;
    st_ = DirectStats{};
    st_.queueMs = q;
    lat_.clear();
    startNs_ = monoNs();
    lastReapNs_ = 0;
}

// ---------------------------------------------------------------------------
// Microfone direto (interface 2, endpoint IN isócrono assíncrono, 2 canais)
// ---------------------------------------------------------------------------
std::vector<int16_t> directRecord(int fd, int endpoint, int seconds, std::string& error) {
    constexpr int kUrbs = 4;
    constexpr int kPackets = 8;
    constexpr int kPacketBytes = 196;  // wMaxPacketSize do endpoint 0x82
    const size_t wanted = static_cast<size_t>(seconds) * 48000u * 2u;  // amostras (estéreo)
    std::vector<int16_t> out;
    out.reserve(wanted + 1024);

    std::vector<void*> urbs;
    std::vector<std::vector<uint8_t>> bufs(kUrbs, std::vector<uint8_t>(kPackets * kPacketBytes));
    int inFlight = 0;
    for (int i = 0; i < kUrbs; ++i) {
        usbdevfs_urb* u = allocUrb(kPackets);
        urbs.push_back(u);
        setupIsoUrb(u, endpoint, bufs[i].data(), kPackets, kPacketBytes, i);
        if (ioctl(fd, USBDEVFS_SUBMITURB, u) < 0) {
            error = errText("USBDEVFS_SUBMITURB (micro)", errno);
            discardAndDrain(fd, urbs, inFlight);
            for (void* p : urbs) std::free(p);
            return {};
        }
        inFlight++;
    }
    const int64_t deadline = monoNs() + (static_cast<int64_t>(seconds) + 3) * 1'000'000'000LL;
    while (out.size() < wanted && monoNs() < deadline) {
        pollfd p{fd, POLLOUT, 0};
        if (poll(&p, 1, 100) <= 0) continue;
        usbdevfs_urb* done = nullptr;
        int reapErr = 0;
        while (true) {
            if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &done) < 0) {
                reapErr = errno;
                break;
            }
            inFlight--;
            const int idx = static_cast<int>(reinterpret_cast<intptr_t>(done->usercontext));
            for (int k = 0; k < done->number_of_packets; ++k) {
                const int n = static_cast<int>(done->iso_frame_desc[k].actual_length);
                if (done->iso_frame_desc[k].status != 0 || n <= 0) continue;
                const auto* src = reinterpret_cast<const int16_t*>(bufs[static_cast<size_t>(idx)].data() + k * kPacketBytes);
                out.insert(out.end(), src, src + n / 2);
            }
            setupIsoUrb(done, endpoint, bufs[static_cast<size_t>(idx)].data(), kPackets, kPacketBytes, idx);
            if (ioctl(fd, USBDEVFS_SUBMITURB, done) == 0) inFlight++;
        }
        if (reapErr != EAGAIN) {
            error = errText("USBDEVFS_REAPURB (micro)", reapErr);
            break;
        }
    }
    discardAndDrain(fd, urbs, inFlight);
    for (void* p : urbs) std::free(p);
    if (out.empty() && error.empty()) error = "o comando não enviou dados de áudio";
    if (out.size() > wanted) out.resize(wanted);
    return out;
}

} // namespace orimbux
