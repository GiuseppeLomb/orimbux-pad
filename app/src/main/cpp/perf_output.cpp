// Ver perf_output.h.
#include "perf_output.h"

#include <android/log.h>
#include <unistd.h>

#include <chrono>

#include "perf_input.h"
#include "usb_direct.h"
#include "wav.h"

#define LOG_TAG "OrimbuxOutput"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace orimbux {

PerfOutput& PerfOutput::get() {
    static PerfOutput instance;
    return instance;
}

int PerfOutput::mode() const {
    const int m = mode_.load();
    if (m == kOutController && !DirectUsbOut::get().running()) return kOutNone;
    return m;
}

std::string PerfOutput::startController(int fd, int numUrbs, int packetsPerUrb) {
    stop();
    DirectUsbOut& usb = DirectUsbOut::get();
    usb.setSource(kSourceEngine);
    PerfEngine::get().setHapticOutputEnabled(true);
    const std::string err = usb.start(fd, 0x01, numUrbs, packetsPerUrb);
    if (!err.empty()) {
        usb.setSource(kSourceTest);
        return err;
    }
    mode_ = kOutController;
    LOGI("Saída: comando (USB direto, fila de %d ms)", numUrbs * packetsPerUrb);
    return "";
}

std::string PerfOutput::startPhone(int speakerDeviceId, int fd, int numUrbs, int packetsPerUrb, int hapticDelayMs) {
    stop();
    tmp_.assign(4096 * 4, 0.0f);
    hapticToUsb_ = false;
    if (fd >= 0) {
        DirectUsbOut& usb = DirectUsbOut::get();
        usb.setSource(kSourceHapticRing);
        usb.setHapticDelayFrames(std::max(0, hapticDelayMs) * 48);
        if (usb.start(fd, 0x01, numUrbs, packetsPerUrb).empty()) hapticToUsb_ = true;
        else usb.setSource(kSourceTest);
    }
    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setSampleRate(48000)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
        ->setChannelCount(2)
        ->setUsage(oboe::Usage::Media)
        ->setContentType(oboe::ContentType::Music)
        ->setDeviceId(speakerDeviceId)
        ->setDataCallback(this)
        ->setErrorCallback(this);
    const oboe::Result r = b.openStream(phone_);
    if (r != oboe::Result::OK || !phone_) {
        if (hapticToUsb_) DirectUsbOut::get().stop();
        return std::string("Oboe: ") + oboe::convertToText(r);
    }
    phone_->setBufferSizeInFrames(phone_->getFramesPerBurst() * 2);
    PerfEngine::get().setHapticOutputEnabled(hapticToUsb_);
    phone_->requestStart();
    mode_ = kOutPhone;
    LOGI("Saída: altifalantes do telemóvel (háptica pelo USB: %s)", hapticToUsb_ ? "sim" : "não");
    return "";
}

void PerfOutput::stop() {
    if (phone_) {
        phone_->stop();
        phone_->close();
        phone_.reset();
    }
    DirectUsbOut& usb = DirectUsbOut::get();
    if (usb.running()) usb.stop();
    usb.setSource(kSourceTest);
    mode_ = kOutNone;
}

oboe::DataCallbackResult PerfOutput::onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) {
    auto* out = static_cast<float*>(data);
    int done = 0;
    while (done < frames) {
        const int n = std::min(frames - done, 4096);
        PerfEngine::get().render(tmp_.data(), n);
        MonoRing& ring = DirectUsbOut::get().hapticRing();
        const bool hap = hapticToUsb_.load();
        for (int f = 0; f < n; ++f) {
            out[(done + f) * 2 + 0] = tmp_[static_cast<size_t>(f * 4 + 0)];
            out[(done + f) * 2 + 1] = tmp_[static_cast<size_t>(f * 4 + 1)];
            if (hap) ring.push(tmp_[static_cast<size_t>(f * 4 + 2)]);
        }
        done += n;
    }
    return oboe::DataCallbackResult::Continue;
}

void PerfOutput::onErrorAfterClose(oboe::AudioStream* s, oboe::Result error) {
    LOGW("Stream dos altifalantes fechou: %s", oboe::convertToText(error));
    mode_ = kOutNone;
}

// ---------------------------------------------------------------- gravação do set

bool PerfOutput::recStart(int fd) {
    if (recRunning_ || fd < 0) return false;
    recFd_ = fd;
    recBytes_ = 0;
    const std::vector<uint8_t> h = wavHeader16(0);
    if (write(fd, h.data(), h.size()) != static_cast<ssize_t>(h.size())) {
        close(fd);
        recFd_ = -1;
        return false;
    }
    ring_.reset();
    recRunning_ = true;
    writer_ = std::thread([this] { writerLoop(); });
    PerfEngine::get().setRecorder(&ring_);
    PerfInput::get().recording.store(true);
    LOGI("Gravação do set iniciada");
    return true;
}

void PerfOutput::writerLoop() {
    std::vector<int16_t> buf(48000);
    while (recRunning_) {
        const size_t n = ring_.pop(buf.data(), buf.size());
        if (n > 0) {
            const ssize_t w = write(recFd_, buf.data(), n * sizeof(int16_t));
            if (w > 0) recBytes_ += static_cast<uint64_t>(w);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

float PerfOutput::recStop() {
    if (!recRunning_) return 0;
    PerfEngine::get().setRecorder(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));  // o áudio deixa de escrever no anel
    recRunning_ = false;
    if (writer_.joinable()) writer_.join();
    std::vector<int16_t> buf(48000);
    size_t n;
    while ((n = ring_.pop(buf.data(), buf.size())) > 0) {
        const ssize_t w = write(recFd_, buf.data(), n * sizeof(int16_t));
        if (w > 0) recBytes_ += static_cast<uint64_t>(w);
    }
    const std::vector<uint8_t> h = wavHeader16(static_cast<uint32_t>(std::min<uint64_t>(recBytes_, 0xFFFFFFF0u)));
    pwrite(recFd_, h.data(), h.size(), 0);  // cabeçalho com o tamanho final
    close(recFd_);
    recFd_ = -1;
    PerfInput::get().recording.store(false);
    const float secs = static_cast<float>(recBytes_) / (48000.0f * 4.0f);
    LOGI("Gravação do set terminada: %.1f s", secs);
    return secs;
}

} // namespace orimbux
