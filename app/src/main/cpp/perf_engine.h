// Motor de atuação do Orimbux Pad.
//
// Corre dentro do callback de áudio (thread do USB isócrono, ou do Oboe no modo
// "altifalantes"). Produz 4 canais por frame: jack L/R (música) + háptica L/R.
// Nada aqui aloca memória nem bloqueia: os comandos chegam por uma fila sem locks
// (SPSC) a partir da thread do comando, e o estado sai por variáveis atómicas.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "dsp.h"
#include "pack.h"

namespace orimbux {

// Fila sem locks com um produtor e um consumidor.
template <typename T, int N>
class SpscQueue {
public:
    bool push(const T& v) {
        const int h = head_.load(std::memory_order_relaxed);
        const int n = (h + 1) % N;
        if (n == tail_.load(std::memory_order_acquire)) return false;
        buf_[h] = v;
        head_.store(n, std::memory_order_release);
        return true;
    }
    bool pop(T& v) {
        const int t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        v = buf_[t];
        tail_.store((t + 1) % N, std::memory_order_release);
        return true;
    }

private:
    std::array<T, N> buf_{};
    std::atomic<int> head_{0}, tail_{0};
};

enum LoopState : int { kLoopOff = 0, kLoopPendingOn = 1, kLoopOn = 2, kLoopPendingOff = 3 };
enum FxType : int { kFxNone = 0, kFxEcho = 1, kFxReverb = 2, kFxFlanger = 3, kFxBitcrush = 4 };

// Saída para a gravação do set (anel SPSC de amostras int16 estéreo).
class RecordRing {
public:
    explicit RecordRing(int frames) : buf_(static_cast<size_t>(frames) * 2) {}
    void push(float l, float r) {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t n = (h + 2) % buf_.size();
        if (n == tail_.load(std::memory_order_acquire)) {
            overflow_++;
            return;
        }
        buf_[h] = toI16(l);
        buf_[h + 1] = toI16(r);
        head_.store(n, std::memory_order_release);
    }
    // Thread de escrita: copia até `max` amostras (int16) e devolve quantas.
    size_t pop(int16_t* out, size_t max) {
        size_t t = tail_.load(std::memory_order_relaxed);
        const size_t h = head_.load(std::memory_order_acquire);
        size_t n = 0;
        while (t != h && n + 2 <= max) {
            out[n++] = buf_[t];
            out[n++] = buf_[t + 1];
            t = (t + 2) % buf_.size();
        }
        tail_.store(t, std::memory_order_release);
        return n;
    }
    int overflows() const { return overflow_.load(); }
    // Só quando ninguém está a escrever (gravação parada).
    void reset() {
        tail_.store(head_.load());
        overflow_.store(0);
    }

private:
    static int16_t toI16(float x) { return static_cast<int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767.0f); }
    std::vector<int16_t> buf_;
    std::atomic<size_t> head_{0}, tail_{0};
    std::atomic<int> overflow_{0};
};

class PerfEngine {
public:
    static PerfEngine& get();

    // ----- chamados pela thread do comando (um único produtor) -----
    void padOn(int pad, float gain);        // one-shot com o ganho da dinâmica (linear)
    void padOff(int pad);                   // largar (modo gate → fade-out de 15 ms)
    void padRefine(int pad, float gain);    // ajuste do ganho nos primeiros ms (report seguinte)
    void loopToggle(int pad);               // liga/desliga no compasso seguinte
    void drop();
    void restartLoops();                    // PS: recomeça no tempo 1 seguinte
    void rewind();
    void toggleMute();

    // ----- parâmetros contínuos (qualquer thread) -----
    void setFilter(float x) { filter_.store(x); }                // −1 low-pass … 0 … +1 high-pass
    void setCuts(uint32_t padMask) { cuts_.store(padMask); }     // pads cortados (stick direito)
    void setRoll(int div, int notePad) { rollDiv_.store(div); rollPad_.store(notePad); }  // div 0 = sem roll
    void setEchoOut(float a) { echoOut_.store(a); }
    void setFx(int type, float amount) { fxType_.store(type); fxAmount_.store(amount); }
    void setVolumes(float loops, float oneshots) { volLoops_.store(loops); volOne_.store(oneshots); }

    // ----- definições (UI) -----
    void setPadGate(int pad, bool gate) { if (pad >= 0 && pad < kPads) gate_[pad].store(gate); }
    void setHaptics(float maxLevel, bool metronomeOn, float metronomeLevel, int profile) {
        hapMax_.store(maxLevel); metroOn_.store(metronomeOn); metroLevel_.store(metronomeLevel); hapProfile_.store(profile);
    }
    void setHapticOutputEnabled(bool on) { hapOut_.store(on); }

    // Troca de pack: `quantized` = no tempo 1 seguinte; senão no próximo bloco (ex.: som gravado).
    // Devolve false se ainda houver uma troca pendente. O pack antigo fica em retiredPack() para apagar.
    bool setPack(Pack* p, bool quantized);
    Pack* takeRetiredPack();  // a thread de UI apaga-o (fora do áudio)
    Pack* currentPack() const { return current_.load(); }

    void setRecorder(RecordRing* r) { recorder_.store(r); }

    // ----- áudio -----
    void render(float* out4, int frames);

    // ----- estado (luzes / UI), atualizado a cada bloco -----
    struct Status {
        std::atomic<int> barInPhrase{0}, beatInBar{0};
        std::atomic<int> beatCount{0};          // conta todos os tempos (para piscar)
        std::atomic<int64_t> lastBeatFrame{0};
        std::atomic<float> beatPhase{0};        // 0..1 dentro do tempo
        std::atomic<float> bpm{172};
        std::atomic<int> loopState[kPads]{};
        std::atomic<float> padLevel[kPads]{};   // envelope de cada pad (ataque 10 ms, release 250 ms)
        std::atomic<int> flashCount[kPads]{};   // incrementa a cada one-shot
        std::atomic<float> flashGain[kPads]{};
        std::atomic<float> masterEnv{0};
        std::atomic<float> brightness{0};       // 0 = graves dominam … 1 = agudos dominam
        std::atomic<int> rollSteps{0};          // incrementa a cada repetição do roll
        std::atomic<int> dropState{0};          // 0 nada · 1 pendente · 2 em silêncio
        std::atomic<int> rewinding{0};
        std::atomic<int> muted{0};
        std::atomic<int> restartPending{0};
        std::atomic<int> packPending{0};
        std::atomic<int> hasPack{0};
        std::atomic<int64_t> framePos{0};
    };
    Status status;

private:
    PerfEngine();

    struct Cmd {
        int type = 0;
        int pad = 0;
        float value = 0;
    };
    enum CmdType : int { kCmdPadOn, kCmdPadOff, kCmdPadRefine, kCmdLoop, kCmdDrop, kCmdRestart, kCmdRewind, kCmdMute };

    struct Voice {
        const SlotData* slot = nullptr;
        const Pack* pack = nullptr;
        int pad = -1;
        int pos = 0;
        float gain = 0, target = 0;
        int release = -1;  // frames restantes do fade-out (−1 = não está a sair)
        int releaseLen = 1;
        bool active = false;
        int64_t startFrame = 0;
    };
    struct Impulse {
        int pos = -1;
        int len = 0;
        float amp = 0;
        double phase = 0;
    };
    struct LoopPlayer {
        int state = kLoopOff;
        dsp::Smoother fade;
    };

    void drainCommands();
    void startVoice(int pad, float gain);
    void startImpulse(float amp, int lengthFrames);
    void onBeat();
    void onBarStart();
    void updateRoll();
    void recomputeTiming();
    float impulseSample(Impulse& im, int profile);
    void retireIfFree();

    SpscQueue<Cmd, 256> cmds_;

    std::atomic<Pack*> current_{nullptr};
    std::atomic<Pack*> pending_{nullptr};
    std::atomic<bool> pendingQuantized_{true};
    std::atomic<Pack*> retired_{nullptr};
    Pack* oldPack_ = nullptr;  // pack anterior cujas vozes ainda tocam

    // Transporte
    int64_t pos_ = 0;
    int64_t anchor_ = 0;     // frame onde começa o compasso 1 da frase
    double fpb_ = 16744.0;   // frames por tempo
    int64_t nextBeat_ = 0;
    int64_t beatIndex_ = 0;  // tempos desde o anchor
    bool restartPending_ = false;
    int dropState_ = 0;      // 0 · 1 pendente · 2 ativo

    std::array<Voice, 40> voices_{};
    std::array<LoopPlayer, kPads> loops_{};
    std::array<dsp::Smoother, kPads> cutGain_{};
    dsp::Smoother dropGain_, muteGain_, liveLoops_, volLoopsSm_, volOneSm_;
    float lastGain_[kPads] = {};

    // Roll / note repeat
    std::vector<float> capL_, capR_;   // captura da música (4 s), para o beat roll e o rewind
    std::vector<float> rollL_, rollR_; // fatia repetida (até 1 s)
    int rollDivActive_ = 0;
    int rollPadActive_ = -1;
    int64_t rollStart_ = 0;
    int rollLen_ = 0;
    int64_t nextRollStep_ = 0;
    bool rollCopied_ = false;
    dsp::Smoother rollMix_;

    // Rewind
    int rewindPhase_ = 0;  // 0 nada · 1 a rodar para trás · 2 silêncio até ao tempo 1
    double rewindRead_ = 0;
    int rewindT_ = 0, rewindLen_ = 1;

    // Efeitos
    dsp::Biquad lp_, hp_;
    dsp::Smoother filterSm_;
    dsp::DelayLine echo_, echoOutDl_, flanger_;
    dsp::Reverb reverb_;
    dsp::Smoother echoAmt_, revAmt_, flgAmt_, crushAmt_, echoOutSm_;
    double flgLfo_ = 0;
    float crushHoldL_ = 0, crushHoldR_ = 0;
    int crushCount_ = 0;

    // Háptica
    dsp::OnePoleLP hapLp1_, hapLp2_;
    std::array<Impulse, 12> impulses_{};

    // Análise
    float padAbs_[kPads] = {};
    float padEnv_[kPads] = {};
    dsp::Envelope masterEnv_;
    dsp::OnePoleLP splitLp_;
    float eLow_ = 0, eHigh_ = 0;

    // Parâmetros atómicos
    std::atomic<float> filter_{0}, echoOut_{0}, fxAmount_{0}, volLoops_{0.85f}, volOne_{0.85f};
    std::atomic<int> fxType_{0}, rollDiv_{0}, rollPad_{-1};
    std::atomic<uint32_t> cuts_{0};
    std::atomic<bool> gate_[kPads]{};
    std::atomic<float> hapMax_{0.4f}, metroLevel_{0.5f};
    std::atomic<bool> metroOn_{true}, hapOut_{true};
    std::atomic<int> hapProfile_{1};
    std::atomic<RecordRing*> recorder_{nullptr};
    bool muted_ = false;
};

} // namespace orimbux
