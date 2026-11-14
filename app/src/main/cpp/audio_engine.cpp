// Ver audio_engine.h.
#include "audio_engine.h"

#include <android/log.h>
#include <oboe/OboeExtensions.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>

#include "clock.h"

#define LOG_TAG "OrimbuxAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace orimbux {

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr float kSr = static_cast<float>(Engine::kSampleRate);
constexpr double kTestBpm = 172.0;
constexpr double kEighthFrames = Engine::kSampleRate * 60.0 / kTestBpm / 2.0;
constexpr int kMaxStats = 64;
constexpr uint32_t kMaskCh12 = 0b0011;   // canais 1–2 (índices 0,1)
constexpr uint32_t kMaskCh34 = 0b1100;   // canais 3–4 (índices 2,3): háptica do DualSense
constexpr uint32_t kMaskCh2 = 0b0010;    // canal 2 (R): o altifalante do comando toca este
constexpr int ms(float m) { return static_cast<int>(m * kSr / 1000.0f); }
} // namespace

// ===========================================================================
// OutputPath
// ===========================================================================

OutputPath::OutputPath(Engine& engine, int pathId) : id(pathId), engine_(engine) {
    latMs_.reserve(kMaxStats);
    cbMs_.reserve(kMaxStats);
}

void OutputPath::resetForStart(int ch) {
    channels = ch;
    framesRendered_ = 0;
    lastTriggerSeq_ = engine_.triggerSeq();
    for (int t = 0; t < kNumTones; ++t) tonesHandled_[t] = engine_.toneRequests(id, t);
    for (auto& v : voices_) v.len = 0;
    musicWasOn_ = false;
    std::lock_guard<std::mutex> lock(measMutex_);
    pendingValid_ = false;
    tsValid_ = false;
}

// Canais onde vai a "música" neste path, conforme o cenário.
uint32_t OutputPath::musicMask(int ch) const {
    const int s = engine_.scenario();
    if (id == kPathPhone) return s == kScenarioA ? kMaskCh12 : 0;
    if (id == kPathCtrl) return (s == kScenarioB && ch >= 2) ? kMaskCh12 : 0;
    return 0;
}

// Canais hápticos (3–4) do DualSense: só existem se o stream tiver 4 canais.
uint32_t OutputPath::hapticMask(int ch) const {
    const int s = engine_.scenario();
    if (id == kPathCtrl && ch >= 4 && (s == kScenarioA || s == kScenarioB)) return kMaskCh34;
    return 0;
}

void OutputPath::startVoice(int type, uint32_t chanMask, float amp, float freq, int lenFrames) {
    if (chanMask == 0 || lenFrames <= 0) return;
    // Procura uma voz livre; se não houver, rouba a mais antiga.
    Voice* slot = nullptr;
    int oldestPos = -1;
    for (auto& v : voices_) {
        if (v.pos >= v.len) { slot = &v; break; }
        if (v.pos > oldestPos) { oldestPos = v.pos; slot = &v; }
    }
    *slot = Voice{};
    slot->type = type;
    slot->chanMask = chanMask;
    slot->amp = amp;
    slot->freq = freq;
    slot->len = lenFrames;
    slot->noise = 0x9E3779B9u ^ static_cast<uint32_t>(framesRendered_);
}

float OutputPath::voiceSample(Voice& v) {
    const float t = static_cast<float>(v.pos) / kSr;
    float s = 0.0f;
    float f = v.freq;
    switch (v.type) {
        case 0:  // clique: seno curto com decaimento rápido
            s = std::sin(static_cast<float>(v.phase)) * std::exp(-t / 0.008f);
            break;
        case 1: {  // burst háptico: seno com ataque/relaxamento de 3 ms
            const float tail = static_cast<float>(v.len - v.pos) / kSr;
            const float env = std::min({1.0f, t / 0.003f, tail / 0.003f});
            s = std::sin(static_cast<float>(v.phase)) * env;
            break;
        }
        case 2: {  // tom de teste com fades de 5 ms
            const float tail = static_cast<float>(v.len - v.pos) / kSr;
            const float env = std::min({1.0f, t / 0.005f, tail / 0.005f});
            s = std::sin(static_cast<float>(v.phase)) * env;
            break;
        }
        case 3:  // kick: seno com queda de frequência
            f = 48.0f + 95.0f * std::exp(-t / 0.03f);
            s = std::sin(static_cast<float>(v.phase)) * std::exp(-t / 0.16f);
            break;
        case 4: {  // hat: ruído com decaimento rápido (passa-altas grosseiro)
            v.noise = v.noise * 1664525u + 1013904223u;
            const float n = static_cast<float>(static_cast<int32_t>(v.noise)) / 2147483648.0f;
            s = n * std::exp(-t / 0.018f);
            break;
        }
        default:
            break;
    }
    v.phase += kTwoPi * f / kSr;
    if (v.phase > kTwoPi) v.phase -= kTwoPi;
    v.pos++;
    return s * v.amp;
}

void OutputPath::render(float* out, int frames, int ch) {
    std::memset(out, 0, sizeof(float) * static_cast<size_t>(frames) * static_cast<size_t>(ch));
    const uint32_t mMask = musicMask(ch);
    const uint32_t hMask = hapticMask(ch);

    // 1) Pad carregado? Começa o som no primeiro frame deste bloco e regista a medição.
    const uint32_t seq = engine_.triggerSeq();
    if (seq != lastTriggerSeq_) {
        lastTriggerSeq_ = seq;
        const int64_t t0 = engine_.triggerT0();
        bool produced = false;
        if (mMask) { startVoice(0, mMask, 0.8f, 1500.0f, ms(30)); produced = true; }
        if (hMask) { startVoice(1, hMask, 1.0f, 120.0f, ms(60)); produced = true; }
        if (produced && measMutex_.try_lock()) {
            pendingValid_ = true;
            pendingFrame_ = framesRendered_;
            pendingT0_ = t0;
            pendingCbNs_ = monoNs();
            measMutex_.unlock();
        }
    }

    // 2) Tons de teste pedidos pela interface.
    for (int t = 0; t < kNumTones; ++t) {
        const uint32_t req = engine_.toneRequests(id, t);
        if (req == tonesHandled_[t]) continue;
        tonesHandled_[t] = req;
        switch (t) {
            case kToneClick: startVoice(0, ch >= 2 ? kMaskCh12 : 1u, 0.8f, 1500.0f, ms(30)); break;
            case kToneHaptic: if (ch >= 4) startVoice(1, kMaskCh34, 1.0f, 100.0f, ms(400)); break;
            case kToneSpeaker: if (ch >= 2) startVoice(2, kMaskCh2, 0.6f, 880.0f, ms(1000)); break;
            case kToneJack: if (ch >= 2) startVoice(2, kMaskCh12, 0.3f, 440.0f, ms(1000)); break;
            default: break;
        }
    }

    // 3) "Música de teste": kick em cada tempo e hat nos contratempos, a 172 BPM.
    const bool music = engine_.testMusic() && (mMask || hMask);
    if (music && !musicWasOn_) {
        musicNextEighth_ = static_cast<double>(framesRendered_);
        musicStep_ = 0;
    }
    musicWasOn_ = music;

    for (int i = 0; i < frames; ++i) {
        if (music && static_cast<double>(framesRendered_ + i) >= musicNextEighth_) {
            if (musicStep_ % 2 == 0) {
                startVoice(3, mMask, 0.6f, 140.0f, ms(300));
                startVoice(3, hMask, 0.9f, 140.0f, ms(200));  // o kick também vai para as mãos
            } else {
                startVoice(4, mMask, 0.18f, 0.0f, ms(60));
            }
            musicStep_++;
            musicNextEighth_ += kEighthFrames;
        }
        float* frame = out + static_cast<size_t>(i) * static_cast<size_t>(ch);
        for (auto& v : voices_) {
            if (v.pos >= v.len) continue;
            const float s = voiceSample(v);
            for (int c = 0; c < ch && c < 32; ++c) {
                if (v.chanMask & (1u << c)) frame[c] += s;
            }
        }
        for (int c = 0; c < ch; ++c) frame[c] = std::clamp(frame[c], -1.0f, 1.0f);
    }
    framesRendered_ += frames;
}

oboe::DataCallbackResult OutputPath::onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) {
    render(static_cast<float*>(data), frames, s->getChannelCount());
    return oboe::DataCallbackResult::Continue;
}

void OutputPath::onErrorAfterClose(oboe::AudioStream* s, oboe::Result error) {
    LOGW("Stream %d fechado: %s", id, oboe::convertToText(error));
    disconnected = true;
    running = false;
}

void OutputPath::setTimestamp(int64_t framePos, int64_t timeNs) {
    std::lock_guard<std::mutex> lock(measMutex_);
    tsValid_ = true;
    tsPos_ = framePos;
    tsTime_ = timeNs;
}

void OutputPath::resolvePending(int64_t nowNs) {
    std::lock_guard<std::mutex> lock(measMutex_);
    if (!pendingValid_) return;
    const bool presented = tsValid_ && tsPos_ >= pendingFrame_;
    const bool timedOut = nowNs - pendingCbNs_ > 400'000'000LL;
    if (!presented && !timedOut) return;

    float latency;
    if (tsValid_) {
        // Quando é que o frame do som sai no DAC, segundo o timestamp do sistema.
        const double presentNs = static_cast<double>(tsTime_) +
            static_cast<double>(pendingFrame_ - tsPos_) * 1e9 / Engine::kSampleRate;
        latency = static_cast<float>((presentNs - static_cast<double>(pendingT0_)) / 1e6);
    } else {
        // Sem timestamp: estimativa = atraso até ao callback + tamanho do buffer.
        latency = static_cast<float>(pendingCbNs_ - pendingT0_) / 1e6f +
                  static_cast<float>(bufferFrames.load()) * 1000.0f / kSr;
        estimated_++;
    }
    const float cb = static_cast<float>(pendingCbNs_ - pendingT0_) / 1e6f;
    pendingValid_ = false;
    if (latency < 0.0f || latency > 1000.0f) return;  // valor absurdo: ignora
    if (latMs_.size() >= kMaxStats) {
        latMs_.erase(latMs_.begin());
        cbMs_.erase(cbMs_.begin());
    }
    latMs_.push_back(latency);
    cbMs_.push_back(cb);
}

LatencyStats OutputPath::stats() {
    std::lock_guard<std::mutex> lock(measMutex_);
    LatencyStats st;
    st.count = static_cast<int>(latMs_.size());
    st.estimated = estimated_;
    if (latMs_.empty()) return st;
    st.minMs = *std::min_element(latMs_.begin(), latMs_.end());
    st.maxMs = *std::max_element(latMs_.begin(), latMs_.end());
    float sum = 0, sumCb = 0;
    for (float v : latMs_) sum += v;
    for (float v : cbMs_) sumCb += v;
    st.avgMs = sum / static_cast<float>(latMs_.size());
    st.cbDelayAvgMs = sumCb / static_cast<float>(cbMs_.size());
    st.lastMs = latMs_.back();
    return st;
}

void OutputPath::clearStats() {
    std::lock_guard<std::mutex> lock(measMutex_);
    latMs_.clear();
    cbMs_.clear();
    estimated_ = 0;
    pendingValid_ = false;
}

// ===========================================================================
// Engine
// ===========================================================================

Engine& Engine::get() {
    static Engine instance;
    return instance;
}

Engine::Engine() {
    for (int i = 0; i < kNumPaths; ++i) paths_[i] = std::make_unique<OutputPath>(*this, i);
    for (auto& row : toneReq_)
        for (auto& v : row) v = 0;
    monitor_ = std::thread([this] { monitorLoop(); });
    monitor_.detach();  // vive enquanto o processo viver
}

void Engine::monitorLoop() {
    while (monitorRunning_) {
        {
            std::lock_guard<std::mutex> lock(openMutex_);
            for (auto& p : paths_) {
                if (p->backend == kBackendOboe && p->running && p->stream) {
                    auto r = p->stream->getTimestamp(CLOCK_MONOTONIC);
                    if (r) p->setTimestamp(r.value().position, r.value().timestamp);
                }
            }
        }
        const int64_t now = monoNs();
        for (auto& p : paths_) p->resolvePending(now);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

std::string Engine::openOboe(int pathId, int deviceId, int channels, bool quadMask) {
    if (pathId < 0 || pathId >= kNumPaths) return "ERRO: path inválido";
    closePath(pathId);
    std::lock_guard<std::mutex> lock(openMutex_);
    OutputPath& p = *paths_[pathId];

    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setSampleRate(kSampleRate)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
        ->setUsage(oboe::Usage::Media)
        ->setContentType(oboe::ContentType::Music)
        ->setDeviceId(deviceId)
        ->setDataCallback(&p)
        ->setErrorCallback(&p);
    if (quadMask) {
        b.setChannelMask(oboe::ChannelMask::Quad);
    } else {
        b.setChannelCount(channels);
    }

    std::shared_ptr<oboe::AudioStream> s;
    const oboe::Result r = b.openStream(s);
    if (r != oboe::Result::OK || !s) {
        std::string err = std::string("ERRO ao abrir: ") + oboe::convertToText(r);
        p.description = err;
        return err;
    }
    const int burst = s->getFramesPerBurst();
    s->setBufferSizeInFrames(burst * 2);
    p.stream = s;
    p.disconnected = false;
    p.backend = kBackendOboe;
    p.bufferFrames = s->getBufferSizeInFrames();
    p.resetForStart(s->getChannelCount());

    std::ostringstream d;
    d << "Oboe · " << (quadMask ? "máscara Quad" : "contagem de canais")
      << " · canais=" << s->getChannelCount()
      << " · " << s->getSampleRate() << " Hz"
      << " · disp. pedido=" << deviceId << " obtido=" << s->getDeviceId()
      << " · " << (s->getSharingMode() == oboe::SharingMode::Exclusive ? "exclusivo" : "partilhado")
      << " · " << (s->getPerformanceMode() == oboe::PerformanceMode::LowLatency ? "baixa latência" : "latência normal")
      << " · MMAP=" << (oboe::OboeExtensions::isMMapUsed(s.get()) ? "sim" : "não")
      << " · burst=" << burst << " · buffer=" << s->getBufferSizeInFrames();
    p.description = d.str();

    const oboe::Result st = s->requestStart();
    if (st != oboe::Result::OK) {
        p.description += std::string(" · ERRO ao arrancar: ") + oboe::convertToText(st);
        s->close();
        p.stream.reset();
        p.backend = kBackendNone;
        return p.description;
    }
    p.running = true;
    LOGI("Path %d: %s", pathId, p.description.c_str());
    return p.description;
}

void Engine::beginPull(int pathId, int channels) {
    if (pathId < 0 || pathId >= kNumPaths) return;
    closePath(pathId);
    std::lock_guard<std::mutex> lock(openMutex_);
    OutputPath& p = *paths_[pathId];
    p.backend = kBackendPull;
    p.disconnected = false;
    p.resetForStart(channels);
    p.running = true;
}

void Engine::renderPull(int pathId, float* out, int frames) {
    OutputPath& p = *paths_[pathId];
    p.render(out, frames, p.channels.load());
}

void Engine::pullTimestamp(int pathId, int64_t framePos, int64_t timeNs) {
    paths_[pathId]->setTimestamp(framePos, timeNs);
}

void Engine::closePath(int pathId) {
    std::lock_guard<std::mutex> lock(openMutex_);
    OutputPath& p = *paths_[pathId];
    p.running = false;
    if (p.stream) {
        p.stream->stop();
        p.stream->close();
        p.stream.reset();
    }
    p.backend = kBackendNone;
    p.channels = 0;
}

void Engine::closeAll() {
    for (int i = 0; i < kNumPaths; ++i) closePath(i);
}

void Engine::requestTone(int pathId, int tone) {
    if (pathId < 0 || pathId >= kNumPaths || tone < 0 || tone >= kNumTones) return;
    toneReq_[pathId][tone].fetch_add(1);
}

void Engine::padPressed(int64_t t0MonoNs) {
    triggerT0_.store(t0MonoNs, std::memory_order_relaxed);
    triggerSeq_.fetch_add(1, std::memory_order_release);
}

LatencyStats Engine::stats(int pathId) {
    if (pathId < 0 || pathId >= kNumPaths) return {};
    return paths_[pathId]->stats();
}

void Engine::clearStats() {
    for (auto& p : paths_) p->clearStats();
}

std::string Engine::status(int pathId) {
    if (pathId < 0 || pathId >= kNumPaths) return "";
    std::lock_guard<std::mutex> lock(openMutex_);
    OutputPath& p = *paths_[pathId];
    std::ostringstream d;
    if (p.disconnected) d << "DESLIGADO (o dispositivo desapareceu) · ";
    switch (p.backend.load()) {
        case kBackendOboe:
            d << p.description;
            if (p.stream) d << " · xruns=" << p.stream->getXRunCount().value();
            break;
        case kBackendPull:
            d << "AudioTrack (Java) · canais=" << p.channels.load();
            break;
        default:
            d << "fechado";
            break;
    }
    return d.str();
}

} // namespace orimbux
