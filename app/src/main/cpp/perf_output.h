// Saídas de áudio da app de atuação + gravação do set.
//   - Comando (por defeito): USB direto (usb_direct.cpp) com o motor como fonte.
//   - Altifalantes do telemóvel (alternativa): Oboe nos altifalantes; a háptica
//     continua pelo USB do comando, num stream à parte (só canais 3–4).
#pragma once

#include <oboe/Oboe.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "perf_engine.h"

namespace orimbux {

enum OutputMode : int { kOutNone = 0, kOutController = 1, kOutPhone = 2 };

class PerfOutput : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    static PerfOutput& get();

    // fd com a interface de áudio 1 do comando já reclamada (Java). Devolve "" ou o erro.
    std::string startController(int fd, int numUrbs, int packetsPerUrb);
    // Altifalantes: deviceId do altifalante; fd ≥ 0 = háptica pelo USB do comando.
    std::string startPhone(int speakerDeviceId, int fd, int numUrbs, int packetsPerUrb, int hapticDelayMs);
    void stop();
    // kOutNone também quando o USB do comando parou sozinho (cabo retirado).
    int mode() const;

    // Gravação do set em WAV 16 bits 48 kHz. O fd passa a ser da app nativa (fecha-o no fim).
    bool recStart(int fd);
    float recStop();  // devolve os segundos gravados
    bool recording() const { return recRunning_.load(); }

    // Oboe
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) override;
    void onErrorAfterClose(oboe::AudioStream* s, oboe::Result error) override;

private:
    PerfOutput() = default;
    void writerLoop();

    std::atomic<int> mode_{kOutNone};
    std::shared_ptr<oboe::AudioStream> phone_;
    std::vector<float> tmp_;
    std::atomic<bool> hapticToUsb_{false};

    RecordRing ring_{48000 * 8};
    std::thread writer_;
    std::atomic<bool> recRunning_{false};
    int recFd_ = -1;
    uint64_t recBytes_ = 0;
};

} // namespace orimbux
