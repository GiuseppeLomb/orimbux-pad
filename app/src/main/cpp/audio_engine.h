// Motor de áudio do Raio-X (Oboe).
//
// Duas saídas ("paths"):
//   - PHONE: altifalantes do telemóvel (estéreo).
//   - CTRL : saída USB do DualSense (4 canais: 1–2 jack/altifalante, 3–4 háptica).
//            Pode correr por Oboe (callback) ou por AudioTrack do Java ("pull").
//
// Cenários do teste de latência:
//   A: música no telemóvel + háptica nos canais 3–4 do comando (2 streams em simultâneo)
//   B: música no jack do comando (canais 1–2) + háptica (3–4) num só stream de 4 canais
//
// Latência botão→som: quando um pad é carregado, cada path regista o índice do primeiro
// frame do som e o momento em que o SDL recebeu o botão (t0). Depois, com o timestamp
// de apresentação do sistema (AAudio/AudioTrack: frame N sai no DAC no instante T),
// calcula-se quando esse frame sai de facto: latência = T(frame) − t0.
#pragma once

#include <oboe/Oboe.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace orimbux {

enum PathId : int { kPathPhone = 0, kPathCtrl = 1, kNumPaths = 2 };
enum Scenario : int { kScenarioNone = 0, kScenarioA = 1, kScenarioB = 2 };
enum Backend : int { kBackendNone = 0, kBackendOboe = 1, kBackendPull = 2 };
enum ToneType : int { kToneClick = 0, kToneHaptic = 1, kToneSpeaker = 2, kToneJack = 3, kNumTones = 4 };

struct LatencyStats {
    int count = 0;
    float minMs = 0, avgMs = 0, maxMs = 0, lastMs = 0;
    float cbDelayAvgMs = 0;  // do botão até o callback de áudio começar o som
    int estimated = 0;       // nº de medições sem timestamp do sistema (estimativa)
};

class Engine;

// Uma saída de áudio com os seus geradores de som de teste.
class OutputPath : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    explicit OutputPath(Engine& engine, int id);

    // Oboe
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) override;
    void onErrorAfterClose(oboe::AudioStream* s, oboe::Result error) override;

    void render(float* out, int frames, int channels);
    void resetForStart(int channels);

    // Medição
    void setTimestamp(int64_t framePos, int64_t timeNs);
    void resolvePending(int64_t nowNs);
    LatencyStats stats();
    void clearStats();

    const int id;
    std::shared_ptr<oboe::AudioStream> stream;
    std::atomic<int> backend{kBackendNone};
    std::atomic<int> channels{0};
    std::atomic<bool> running{false};
    std::atomic<bool> disconnected{false};
    std::atomic<int> bufferFrames{0};
    std::string description;  // texto para o relatório

private:
    struct Voice {
        int type = 0;          // 0 clique · 1 burst háptico · 2 tom · 3 kick · 4 hat
        uint32_t chanMask = 0;
        float amp = 0;
        float freq = 0;
        double phase = 0;
        int len = 0;
        int pos = 0;
        uint32_t noise = 12345;
    };
    void startVoice(int type, uint32_t chanMask, float amp, float freq, int lenFrames);
    float voiceSample(Voice& v);
    uint32_t musicMask(int ch) const;
    uint32_t hapticMask(int ch) const;

    Engine& engine_;
    std::array<Voice, 16> voices_{};
    int64_t framesRendered_ = 0;
    uint32_t lastTriggerSeq_ = 0;
    std::array<uint32_t, kNumTones> tonesHandled_{};
    double musicNextEighth_ = 0;
    int musicStep_ = 0;
    bool musicWasOn_ = false;

    // Medição pendente (escrita no callback com try_lock; lida pelo monitor).
    std::mutex measMutex_;
    bool pendingValid_ = false;
    int64_t pendingFrame_ = 0;
    int64_t pendingT0_ = 0;
    int64_t pendingCbNs_ = 0;
    bool tsValid_ = false;
    int64_t tsPos_ = 0;
    int64_t tsTime_ = 0;
    std::vector<float> latMs_;
    std::vector<float> cbMs_;
    int estimated_ = 0;
};

class Engine {
public:
    static Engine& get();
    static constexpr int kSampleRate = 48000;

    // Abre o path com Oboe. quadMask = pede ChannelMask::Quad em vez de "4 canais".
    std::string openOboe(int pathId, int deviceId, int channels, bool quadMask);
    // Path alimentado pelo Java (AudioTrack com channel index mask).
    void beginPull(int pathId, int channels);
    void renderPull(int pathId, float* out, int frames);
    void pullTimestamp(int pathId, int64_t framePos, int64_t timeNs);
    void closePath(int pathId);
    void closeAll();

    void setScenario(int s) { scenario_ = s; }
    int scenario() const { return scenario_; }
    void setTestMusic(bool on) { testMusic_ = on; }
    bool testMusic() const { return testMusic_; }
    void requestTone(int pathId, int tone);
    uint32_t toneRequests(int pathId, int tone) const { return toneReq_[pathId][tone].load(); }

    // Chamado pela thread do comando quando um pad é carregado.
    void padPressed(int64_t t0MonoNs);
    uint32_t triggerSeq() const { return triggerSeq_.load(std::memory_order_acquire); }
    int64_t triggerT0() const { return triggerT0_.load(std::memory_order_relaxed); }

    LatencyStats stats(int pathId);
    void clearStats();
    std::string status(int pathId);

private:
    Engine();
    void monitorLoop();

    std::array<std::unique_ptr<OutputPath>, kNumPaths> paths_;
    std::mutex openMutex_;
    std::atomic<int> scenario_{kScenarioNone};
    std::atomic<bool> testMusic_{false};
    std::atomic<uint32_t> triggerSeq_{0};
    std::atomic<int64_t> triggerT0_{0};
    std::array<std::array<std::atomic<uint32_t>, kNumTones>, kNumPaths> toneReq_{};
    std::thread monitor_;
    std::atomic<bool> monitorRunning_{true};
};

} // namespace orimbux
