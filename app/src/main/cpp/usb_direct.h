// Plano B — áudio USB direto para o DualSense, sem passar pelo Android.
//
// No S25 Ultra o Android não regista a saída de áudio do comando (hasOutput:false),
// por isso a app fala diretamente com o endpoint isócrono do comando pelo usbfs do
// Linux (o mesmo mecanismo que a UsbDeviceConnection do Android usa por baixo):
//   - o Java abre uma UsbDeviceConnection, reclama a interface 1 (force=true) e escolhe
//     a alternativa 1; depois passa o descritor de ficheiro (fd) para aqui;
//   - aqui submetem-se URBs isócronos (USBDEVFS_SUBMITURB) e recolhem-se os concluídos
//     (USBDEVFS_REAPURBNDELAY), sempre com vários em fila para não haver falhas.
//
// Formato do endpoint 0x01 (lido no telemóvel): 4 canais × 16 bits × 48 kHz,
// 1 pacote por ms (48 frames = 384 bytes; o máximo é 392).
//   canais 1–2 = jack 3,5 mm · canais 3–4 = atuadores hápticos
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace orimbux {

struct DirectStats {
    int running = 0;
    float elapsedS = 0;
    int64_t urbsDone = 0;
    int64_t packetErrors = 0;   // pacotes com estado de erro
    int64_t underruns = 0;      // a fila de URBs esvaziou (provoca um corte)
    int64_t submitErrors = 0;
    float maxGapMs = 0;         // maior intervalo entre URBs concluídos
    int latCount = 0;
    float latMin = 0, latAvg = 0, latMax = 0, latLast = 0;
    int lastErrno = 0;
    float queueMs = 0;          // tamanho da fila (latência mínima teórica)
};

class DirectUsbOut {
public:
    static DirectUsbOut& get();

    // fd com a interface 1 já reclamada e na alternativa 1. Devolve "" ou o erro.
    std::string start(int fd, int endpoint, int numUrbs, int packetsPerUrb);
    void stop();
    bool running() const { return running_; }

    void setTone(bool on) { tone_ = on; }     // 440 Hz contínuo nos canais 1–2
    void setPulse(bool on) { pulse_ = on; }   // 80 Hz: 120 ms a cada segundo nos canais 3–4
    void setLevel(float jack) { jackLevel_ = jack; }

    // Pad carregado (thread do comando): clique no jack + impulso háptico + medição.
    void padPressed(int64_t t0MonoNs);

    DirectStats stats();
    void clearStats();

private:
    DirectUsbOut() = default;
    void loop();
    void fill(int16_t* dst, int frames, int urbIndex);

    int fd_ = -1;
    int endpoint_ = 0x01;
    int numUrbs_ = 4;
    int packetsPerUrb_ = 2;
    static constexpr int kChannels = 4;
    static constexpr int kFramesPerPacket = 48;  // 48 kHz / 1000 pacotes por segundo
    std::vector<void*> urbs_;
    std::vector<std::vector<int16_t>> buffers_;
    std::vector<int64_t> urbTriggerT0_;          // t0 do pad cujo som começa neste URB (0 = nenhum)

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> tone_{false};
    std::atomic<bool> pulse_{false};
    std::atomic<float> jackLevel_{0.25f};

    std::atomic<uint32_t> triggerSeq_{0};
    std::atomic<int64_t> triggerT0_{0};
    uint32_t lastSeq_ = 0;

    // Estado dos geradores (só na thread de áudio)
    double tonePhase_ = 0;
    float toneGain_ = 0;      // rampa para ligar/desligar sem estalidos
    double pulsePhase_ = 0;
    int64_t frameCounter_ = 0;
    int clickPos_ = -1;
    double clickPhase_ = 0;
    int burstPos_ = -1;
    double burstPhase_ = 0;

    std::mutex statsMutex_;
    DirectStats st_;
    int64_t startNs_ = 0;
    int64_t lastReapNs_ = 0;
    std::vector<float> lat_;
};

// Grava o microfone do comando diretamente (interface 2, endpoint 0x82, 2 canais).
// fd com a interface 2 reclamada e na alternativa 1. Devolve amostras estéreo intercaladas.
std::vector<int16_t> directRecord(int fd, int endpoint, int seconds, std::string& error);

} // namespace orimbux
