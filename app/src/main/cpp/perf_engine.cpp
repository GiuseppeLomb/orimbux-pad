// Ver perf_engine.h.
#include "perf_engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orimbux {

namespace {
constexpr int kCapFrames = 48000 * 4;   // 4 s de captura para o roll e o rewind
constexpr int kRollMax = 48000;         // fatia máxima do roll (1/4 a 48 BPM)
constexpr int kGateFade = 720;          // 15 ms
constexpr int kCutFade = 240;           // 5 ms
constexpr int kMaxVoicesPerPad = 4;
constexpr float kTwoPi = 6.2831853f;

// Peso da háptica de cada pad (o kick e o sub sentem-se mais; agudos menos).
constexpr float kPadHaptic[kPads] = {1.0f, 1.0f, 0.65f, 0.4f, 0.45f, 0.4f, 0.35f, 0.4f};

float softClip(float x) {
    const float a = std::fabs(x);
    if (a < 0.9f) return x;
    return std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) * 10.0f), x);
}
} // namespace

PerfEngine& PerfEngine::get() {
    static PerfEngine instance;
    return instance;
}

PerfEngine::PerfEngine() {
    capL_.assign(kCapFrames, 0.0f);
    capR_.assign(kCapFrames, 0.0f);
    rollL_.assign(kRollMax, 0.0f);
    rollR_.assign(kRollMax, 0.0f);
    echo_.init(48000 * 2);
    echoOutDl_.init(48000 * 2);
    flanger_.init(1024);
    reverb_.init();
    for (auto& l : loops_) l.fade.setTime(0.002f);  // ~5 ms até ~94%
    for (auto& c : cutGain_) {
        c.setTime(0.0015f);
        c.reset(1.0f);
    }
    dropGain_.setTime(0.0015f);
    dropGain_.reset(1.0f);
    muteGain_.setTime(0.0015f);
    muteGain_.reset(1.0f);
    liveLoops_.setTime(0.0015f);
    liveLoops_.reset(1.0f);
    volLoopsSm_.setTime(0.02f);
    volLoopsSm_.reset(0.85f);
    volOneSm_.setTime(0.02f);
    volOneSm_.reset(0.85f);
    rollMix_.setTime(0.0015f);
    filterSm_.setTime(0.03f);
    echoAmt_.setTime(0.06f);
    revAmt_.setTime(0.06f);
    flgAmt_.setTime(0.06f);
    crushAmt_.setTime(0.06f);
    echoOutSm_.setTime(0.03f);
    hapLp1_.set(150.0f);
    hapLp2_.set(150.0f);
    splitLp_.set(800.0f);
    masterEnv_.set(0.010f, 0.250f);
    lp_.lowpass(20000.0f, 0.9f);
    hp_.highpass(20.0f, 0.9f);
    // Por defeito: gate nas setas (stabs, vocais, riser), one-shot no kick, snare, sub e crash.
    for (int p = 0; p < kPads; ++p) gate_[p].store(p >= kPadDown);
}

// ---------------------------------------------------------------- comandos

void PerfEngine::padOn(int pad, float gain) { cmds_.push({kCmdPadOn, pad, gain}); }
void PerfEngine::padOff(int pad) { cmds_.push({kCmdPadOff, pad, 0}); }
void PerfEngine::padRefine(int pad, float gain) { cmds_.push({kCmdPadRefine, pad, gain}); }
void PerfEngine::loopToggle(int pad) { cmds_.push({kCmdLoop, pad, 0}); }
void PerfEngine::drop() { cmds_.push({kCmdDrop, 0, 0}); }
void PerfEngine::restartLoops() { cmds_.push({kCmdRestart, 0, 0}); }
void PerfEngine::rewind() { cmds_.push({kCmdRewind, 0, 0}); }
void PerfEngine::toggleMute() { cmds_.push({kCmdMute, 0, 0}); }

bool PerfEngine::setPack(Pack* p, bool quantized) {
    Pack* expected = nullptr;
    pendingQuantized_.store(quantized);
    if (!pending_.compare_exchange_strong(expected, p)) return false;
    status.packPending.store(1);
    return true;
}

Pack* PerfEngine::takeRetiredPack() { return retired_.exchange(nullptr); }

void PerfEngine::drainCommands() {
    Cmd c;
    while (cmds_.pop(c)) {
        const Pack* pk = current_.load(std::memory_order_relaxed);
        switch (c.type) {
            case kCmdPadOn:
                if (c.pad >= 0 && c.pad < kPads) {
                    lastGain_[c.pad] = c.value;
                    startVoice(c.pad, c.value);
                }
                break;
            case kCmdPadOff:
                if (c.pad >= 0 && c.pad < kPads && gate_[c.pad].load()) {
                    for (auto& v : voices_) {
                        if (v.active && v.pad == c.pad && v.release < 0) {
                            v.release = kGateFade;
                            v.releaseLen = kGateFade;
                        }
                    }
                }
                break;
            case kCmdPadRefine:
                // Só sobe o ganho da voz mais recente desse pad (o pico chegou no report seguinte).
                if (c.pad >= 0 && c.pad < kPads) {
                    Voice* newest = nullptr;
                    for (auto& v : voices_)
                        if (v.active && v.pad == c.pad && (!newest || v.startFrame > newest->startFrame)) newest = &v;
                    if (newest && c.value > newest->target && pos_ - newest->startFrame < 1440) {
                        newest->target = c.value;
                        lastGain_[c.pad] = c.value;
                    }
                }
                break;
            case kCmdLoop:
                if (c.pad >= 0 && c.pad < kPads && pk && pk->loop[c.pad].loaded) {
                    LoopPlayer& lp = loops_[static_cast<size_t>(c.pad)];
                    switch (lp.state) {
                        case kLoopOff: lp.state = kLoopPendingOn; break;
                        case kLoopPendingOn: lp.state = kLoopOff; break;
                        case kLoopOn: lp.state = kLoopPendingOff; break;
                        case kLoopPendingOff: lp.state = kLoopOn; break;
                    }
                }
                break;
            case kCmdDrop:
                if (dropState_ == 0) dropState_ = 1;
                break;
            case kCmdRestart:
                restartPending_ = true;
                break;
            case kCmdRewind:
                if (rewindPhase_ == 0) {
                    rewindPhase_ = 1;
                    rewindRead_ = static_cast<double>(pos_ - 1);
                    rewindT_ = 0;
                    rewindLen_ = static_cast<int>(fpb_);
                    liveLoops_.target = 0.0f;
                    for (auto& v : voices_)
                        if (v.active && v.release < 0) { v.release = kCutFade; v.releaseLen = kCutFade; }
                }
                break;
            case kCmdMute:
                muted_ = !muted_;
                muteGain_.target = muted_ ? 0.0f : 1.0f;
                break;
            default:
                break;
        }
    }
}

void PerfEngine::startVoice(int pad, float gain) {
    const Pack* pk = current_.load(std::memory_order_relaxed);
    if (!pk || !pk->oneshot[pad].loaded) return;
    // Máximo de 4 vozes por pad: a mais antiga corta (com um fade curto).
    int count = 0;
    Voice* oldest = nullptr;
    for (auto& v : voices_) {
        if (v.active && v.pad == pad && v.release < 0) {
            count++;
            if (!oldest || v.startFrame < oldest->startFrame) oldest = &v;
        }
    }
    if (count >= kMaxVoicesPerPad && oldest) {
        oldest->release = kCutFade;
        oldest->releaseLen = kCutFade;
    }
    Voice* slot = nullptr;
    for (auto& v : voices_)
        if (!v.active) { slot = &v; break; }
    if (!slot) {  // tudo ocupado: rouba a mais antiga de todas
        for (auto& v : voices_)
            if (!slot || v.startFrame < slot->startFrame) slot = &v;
    }
    *slot = Voice{};
    slot->slot = &pk->oneshot[pad];
    slot->pack = pk;
    slot->pad = pad;
    slot->gain = gain;
    slot->target = gain;
    slot->active = true;
    slot->startFrame = pos_;

    status.flashCount[pad].fetch_add(1);
    status.flashGain[pad].store(gain);
    // Impulso háptico do one-shot: segue o volume do som (dinâmica) e o peso do pad.
    startImpulse(gain * pk->oneshot[pad].gain * kPadHaptic[pad], 2880);
}

void PerfEngine::startImpulse(float amp, int lengthFrames) {
    if (amp <= 0.001f) return;
    Impulse* slot = nullptr;
    for (auto& im : impulses_)
        if (im.pos < 0) { slot = &im; break; }
    if (!slot) slot = &impulses_[0];
    slot->pos = 0;
    slot->len = lengthFrames;
    slot->amp = std::min(amp, 1.0f);
    slot->phase = 0;
}

// Impulso háptico. Perfil 1 (transiente): 6 ms a 200 Hz com ataque imediato, depois 80 Hz a decair.
// Perfil 0 (suave): 80 Hz com fade-in de 3 ms.
float PerfEngine::impulseSample(Impulse& im, int profile) {
    const double t = im.pos / 48000.0;
    float v;
    if (profile == 1) {
        const bool attack = t < 0.006;
        const float env = attack ? 1.0f : static_cast<float>(std::exp(-(t - 0.006) / 0.02));
        v = static_cast<float>(std::sin(im.phase)) * env;
        im.phase += kTwoPi * (attack ? 200.0 : 80.0) / 48000.0;
    } else {
        const float env = std::min({1.0f, static_cast<float>(im.pos) / 144.0f, static_cast<float>(im.len - im.pos) / 144.0f});
        v = static_cast<float>(std::sin(im.phase)) * env;
        im.phase += kTwoPi * 80.0 / 48000.0;
    }
    const float tail = std::min(1.0f, static_cast<float>(im.len - im.pos) / 48.0f);
    if (++im.pos >= im.len) im.pos = -1;
    return v * im.amp * tail;
}

// ---------------------------------------------------------------- transporte

void PerfEngine::recomputeTiming() {
    const Pack* pk = current_.load(std::memory_order_relaxed);
    const double bpm = pk ? pk->bpm : 172.0;
    fpb_ = 48000.0 * 60.0 / bpm;
    status.bpm.store(static_cast<float>(bpm));
}

void PerfEngine::onBarStart() {
    // Troca de pack quantizada (no tempo 1)
    Pack* p = pending_.load();
    if (p && pendingQuantized_.load()) {
        if (!oldPack_) {
            oldPack_ = current_.exchange(p);
            pending_.store(nullptr);
            status.packPending.store(0);
            recomputeTiming();
            anchor_ = pos_;
            beatIndex_ = 0;
            for (int i = 0; i < kPads; ++i) {
                LoopPlayer& lp = loops_[static_cast<size_t>(i)];
                if (!p->loop[i].loaded) lp.state = kLoopOff;
            }
        }
    }
    if (restartPending_) {
        restartPending_ = false;
        anchor_ = pos_;
        beatIndex_ = 0;
    }
    if (rewindPhase_ == 2) {
        // Depois do rewind: os loops recomeçam do início da frase, no tempo 1.
        rewindPhase_ = 0;
        anchor_ = pos_;
        beatIndex_ = 0;
        liveLoops_.target = 1.0f;
    }
    if (dropState_ == 2) {
        dropState_ = 0;
        dropGain_.target = 1.0f;
    } else if (dropState_ == 1) {
        dropState_ = 2;
        dropGain_.target = 0.0f;
    }
    for (auto& lp : loops_) {
        if (lp.state == kLoopPendingOn) lp.state = kLoopOn;
        else if (lp.state == kLoopPendingOff) lp.state = kLoopOff;
    }
}

void PerfEngine::onBeat() {
    beatIndex_ = static_cast<int64_t>(std::llround(static_cast<double>(nextBeat_ - anchor_) / fpb_));
    if (beatIndex_ % 4 == 0) onBarStart();  // pode mudar o anchor (beatIndex_ volta a 0)
    const int beatInBar = static_cast<int>(beatIndex_ % 4);
    const int barInPhrase = static_cast<int>((beatIndex_ / 4) % 16);
    status.beatInBar.store(beatInBar);
    status.barInPhrase.store(barInPhrase);
    status.beatCount.fetch_add(1);
    status.lastBeatFrame.store(pos_);

    // Háptica: pulso forte no tempo 1 de cada frase; metrónomo em cada tempo (mais forte no tempo 1).
    const bool phraseStart = beatInBar == 0 && barInPhrase == 0;
    float amp = 0.0f;
    if (metroOn_.load()) amp = metroLevel_.load() * (beatInBar == 0 ? 0.65f : 0.35f);
    if (phraseStart) amp = std::max(amp, 1.0f);
    if (!current_.load(std::memory_order_relaxed) || rewindPhase_ != 0) amp = 0.0f;  // sem pack / em rewind: sem pulsos
    if (amp > 0) startImpulse(amp, phraseStart ? 4800 : 1920);

    nextBeat_ = anchor_ + static_cast<int64_t>(std::llround(static_cast<double>(beatIndex_ + 1) * fpb_));
}

// Liga/desliga/muda o roll conforme o pedido do comando (R2).
void PerfEngine::updateRoll() {
    const int div = rollDiv_.load();
    const int pad = rollPad_.load();
    if (div == rollDivActive_ && pad == rollPadActive_) return;
    const bool wasWholeMix = rollDivActive_ > 0 && rollPadActive_ < 0;
    rollDivActive_ = div;
    rollPadActive_ = pad;
    if (div <= 0) {
        rollMix_.target = 0.0f;
        return;
    }
    static const double kDivBeats[5] = {1.0, 1.0, 0.5, 0.25, 0.125};  // 1/4, 1/8, 1/16, 1/32
    rollLen_ = std::clamp(static_cast<int>(std::lround(fpb_ * kDivBeats[std::clamp(div, 1, 4)])), 64, kRollMax);
    const int64_t since = pos_ - anchor_;
    const int64_t steps = since / rollLen_;
    rollStart_ = anchor_ + steps * rollLen_;
    nextRollStep_ = rollStart_ + rollLen_;
    rollCopied_ = false;
    rollMix_.target = pad < 0 ? 1.0f : 0.0f;
    if (!wasWholeMix && pad < 0) rollMix_.value = std::min(rollMix_.value, 1.0f);
}

void PerfEngine::retireIfFree() {
    if (!oldPack_) return;
    for (const auto& v : voices_)
        if (v.active && v.pack == oldPack_) return;
    Pack* expected = nullptr;
    if (retired_.compare_exchange_strong(expected, oldPack_)) oldPack_ = nullptr;
}

// ---------------------------------------------------------------- áudio

void PerfEngine::render(float* out, int frames) {
    drainCommands();

    // Troca imediata (ex.: som gravado pelo micro substitui um one-shot)
    Pack* p = pending_.load();
    if (p && !pendingQuantized_.load() && !oldPack_) {
        oldPack_ = current_.exchange(p);
        pending_.store(nullptr);
        status.packPending.store(0);
        recomputeTiming();
    } else if (p && !current_.load() && !oldPack_) {
        current_.store(p);  // primeiro pack: entra já
        pending_.store(nullptr);
        status.packPending.store(0);
        recomputeTiming();
        anchor_ = pos_;
        nextBeat_ = pos_;
    }
    retireIfFree();
    updateRoll();

    const Pack* pk = current_.load(std::memory_order_relaxed);
    const uint32_t cuts = cuts_.load();
    for (int i = 0; i < kPads; ++i) cutGain_[static_cast<size_t>(i)].target = (cuts >> i) & 1u ? 0.0f : 1.0f;
    volLoopsSm_.target = volLoops_.load();
    volOneSm_.target = volOne_.load();
    echoOutSm_.target = echoOut_.load();
    filterSm_.target = filter_.load();
    {
        const int fx = fxType_.load();
        const float amt = std::clamp(fxAmount_.load(), 0.0f, 1.0f);
        echoAmt_.target = fx == kFxEcho ? amt : 0.0f;
        revAmt_.target = fx == kFxReverb ? amt : 0.0f;
        flgAmt_.target = fx == kFxFlanger ? amt : 0.0f;
        crushAmt_.target = fx == kFxBitcrush ? amt : 0.0f;
        // Ao largar o R1, o efeito desvanece em ~200 ms; ao entrar é mais rápido.
        for (dsp::Smoother* s : {&echoAmt_, &revAmt_, &flgAmt_, &crushAmt_}) s->setTime(s->target < s->value ? 0.07f : 0.02f);
    }
    const float hapMax = std::clamp(hapMax_.load(), 0.0f, 1.0f);
    const int hapProfile = hapProfile_.load();
    const bool hapOut = hapOut_.load();
    RecordRing* rec = recorder_.load();
    const int echoDelay = std::clamp(static_cast<int>(fpb_ * 0.75), 1, echo_.size() - 1);  // 1/8 ponteado
    for (float& a : padAbs_) a = 0.0f;

    for (int f = 0; f < frames; ++f) {
        if (pos_ >= nextBeat_) onBeat();

        // Filtro: coeficientes a cada 32 frames (curva logarítmica, Q fixo 0,9)
        if ((f & 31) == 0) {
            const float x = filterSm_.value;
            const float lpf = x < 0 ? 20000.0f * std::pow(80.0f / 20000.0f, -x) : 20000.0f;
            const float hpf = x > 0 ? 20.0f * std::pow(8000.0f / 20.0f, x) : 20.0f;
            lp_.lowpass(lpf, 0.9f);
            hp_.highpass(hpf, 0.9f);
        }
        filterSm_.next();

        float loopsL = 0, loopsR = 0, oneL = 0, oneR = 0, hapIn = 0;
        const float liveLoops = liveLoops_.next() * dropGain_.next();

        // ---- loops (sincronizados à grelha a partir do anchor) ----
        if (pk) {
            for (int i = 0; i < kPads; ++i) {
                LoopPlayer& lp = loops_[static_cast<size_t>(i)];
                const bool audible = lp.state == kLoopOn || lp.state == kLoopPendingOff;
                lp.fade.target = audible ? 1.0f : 0.0f;
                const float fg = lp.fade.next();
                const float cg = cutGain_[static_cast<size_t>(i)].next();
                if (fg < 1e-4f) continue;
                const SlotData& s = pk->loop[i];
                if (!s.loaded || s.loopFrames <= 0) continue;
                const int idx = static_cast<int>((pos_ - anchor_) % s.loopFrames);
                float l = s.audio.l[static_cast<size_t>(idx)];
                float r = s.audio.r[static_cast<size_t>(idx)];
                if (idx < s.tailFrames) {
                    // Crossfade de 5 ms: a cauda do fim entra no início (loop sem cliques).
                    const float w = static_cast<float>(idx) / static_cast<float>(s.tailFrames);
                    const size_t ti = static_cast<size_t>(s.loopFrames + idx);
                    l = l * w + s.audio.l[ti] * (1.0f - w);
                    r = r * w + s.audio.r[ti] * (1.0f - w);
                }
                const float g = fg * cg * s.gain * liveLoops;
                l *= g;
                r *= g;
                loopsL += l;
                loopsR += r;
                padAbs_[i] += std::fabs(l) + std::fabs(r);
                if (i == kPadCross || i == kPadSquare) hapIn += (l + r) * 0.5f;
            }
        } else {
            for (auto& c : cutGain_) c.next();
        }

        // ---- one-shots ----
        for (auto& v : voices_) {
            if (!v.active) continue;
            const SlotData& s = *v.slot;
            if (v.pos >= s.audio.frames()) {
                v.active = false;
                continue;
            }
            v.gain += (v.target - v.gain) * 0.02f;  // ~1 ms: ajuste suave do ganho (refinamento da dinâmica)
            float g = v.gain * s.gain * cutGain_[static_cast<size_t>(v.pad)].value;
            if (v.release >= 0) {
                g *= static_cast<float>(v.release) / static_cast<float>(v.releaseLen);
                if (--v.release <= 0) v.active = false;
            }
            const float l = s.audio.l[static_cast<size_t>(v.pos)] * g;
            const float r = s.audio.r[static_cast<size_t>(v.pos)] * g;
            oneL += l;
            oneR += r;
            padAbs_[v.pad] += std::fabs(l) + std::fabs(r);
            if (v.pad == kPadCross || v.pad == kPadSquare) hapIn += (l + r) * 0.5f;
            v.pos++;
        }

        const float vl = volLoopsSm_.next(), vo = volOneSm_.next();
        float mL = loopsL * vl + oneL * vo;
        float mR = loopsR * vl + oneR * vo;

        // ---- captura + beat roll (toda a música) / note repeat (só um pad) ----
        const size_t ci = static_cast<size_t>(pos_ % kCapFrames);
        capL_[ci] = mL;
        capR_[ci] = mR;
        if (rollDivActive_ > 0 && pos_ >= nextRollStep_) {
            status.rollSteps.fetch_add(1);
            if (rollPadActive_ >= 0) startVoice(rollPadActive_, lastGain_[rollPadActive_]);
            nextRollStep_ += rollLen_;
        }
        const float rm = rollMix_.next();
        if (rm > 1e-4f) {
            const int64_t off = (pos_ - rollStart_) % std::max(rollLen_, 1);
            float rl, rr;
            if (pos_ - rollStart_ < rollLen_) {
                rl = mL;
                rr = mR;  // primeira passagem: é a própria música ao vivo
            } else {
                if (!rollCopied_) {  // guarda a fatia (a captura de 4 s pode ser reescrita)
                    for (int k = 0; k < rollLen_; ++k) {
                        const size_t src = static_cast<size_t>((rollStart_ + k) % kCapFrames);
                        rollL_[static_cast<size_t>(k)] = capL_[src];
                        rollR_[static_cast<size_t>(k)] = capR_[src];
                    }
                    rollCopied_ = true;
                }
                rl = rollL_[static_cast<size_t>(off)];
                rr = rollR_[static_cast<size_t>(off)];
            }
            mL = mL * (1.0f - rm) + rl * rm;
            mR = mR * (1.0f - rm) + rr * rm;
        }

        // ---- rewind estilo jungle: vinil a travar e a andar para trás (~1 tempo) ----
        if (rewindPhase_ == 1) {
            const double t = static_cast<double>(rewindT_) / rewindLen_;
            const double speed = 1.0 - 5.0 * std::pow(t, 0.8);  // de +1 (normal) até −4 (para trás, rápido)
            rewindRead_ += speed;
            const double rp = std::fmod(rewindRead_ + kCapFrames * 4.0, static_cast<double>(kCapFrames));
            const size_t a = static_cast<size_t>(rp);
            const size_t b = (a + 1) % kCapFrames;
            const float fr = static_cast<float>(rp - std::floor(rp));
            const float g = static_cast<float>(std::min(1.0, (1.0 - t) * 5.0));
            mL += (capL_[a] + (capL_[b] - capL_[a]) * fr) * g;
            mR += (capR_[a] + (capR_[b] - capR_[a]) * fr) * g;
            if (++rewindT_ >= rewindLen_) rewindPhase_ = 2;  // silêncio até ao próximo tempo 1
        }

        // ---- filtro de DJ bipolar ----
        mL = hp_.process(lp_.process(mL, 0), 0);
        mR = hp_.process(lp_.process(mR, 1), 1);

        // ---- echo out (L2): envio para delay com feedback alto, seco a baixar ----
        const float eo = echoOutSm_.next();
        {
            float dl, dr;
            echoOutDl_.read(echoDelay, dl, dr);
            echoOutDl_.write(mL * eo + dl * 0.72f, mR * eo + dr * 0.72f);
            const float dry = 1.0f - 0.9f * eo;
            mL = mL * dry + dl;
            mR = mR * dry + dr;
        }

        // ---- efeitos do giroscópio (R1 + inclinar) ----
        const float ea = echoAmt_.next();
        {
            float dl, dr;
            echo_.read(echoDelay, dl, dr);
            echo_.write(mL * ea * 0.8f + dl * 0.45f, mR * ea * 0.8f + dr * 0.45f);
            mL += dl;
            mR += dr;
        }
        const float ra = revAmt_.next();
        if (ra > 1e-4f) {
            float wl, wr;
            reverb_.process(mL * ra, mR * ra, wl, wr);
            mL += wl;
            mR += wr;
        } else {
            float wl, wr;
            reverb_.process(0, 0, wl, wr);  // deixa a cauda acabar naturalmente
            mL += wl;
            mR += wr;
        }
        const float fa = flgAmt_.next();
        {
            flgLfo_ += kTwoPi * 0.3 / 48000.0;
            if (flgLfo_ > kTwoPi) flgLfo_ -= kTwoPi;
            const float d = 48.0f * (0.6f + 2.4f * (0.5f + 0.5f * static_cast<float>(std::sin(flgLfo_))));
            float yl, yr;
            flanger_.readFrac(d, yl, yr);
            flanger_.write(mL + yl * 0.55f * fa, mR + yr * 0.55f * fa);
            mL = mL * (1.0f - 0.4f * fa) + yl * 0.8f * fa;
            mR = mR * (1.0f - 0.4f * fa) + yr * 0.8f * fa;
        }
        const float ka = crushAmt_.next();
        if (ka > 1e-3f) {
            const int hold = 1 + static_cast<int>(ka * 11.0f);
            if (++crushCount_ >= hold) {
                crushCount_ = 0;
                const float levels = std::pow(2.0f, 16.0f - ka * 12.0f);
                crushHoldL_ = std::round(mL * levels) / levels;
                crushHoldR_ = std::round(mR * levels) / levels;
            }
            const float mix = std::min(1.0f, ka * 2.0f);
            mL = mL * (1.0f - mix) + crushHoldL_ * mix;
            mR = mR * (1.0f - mix) + crushHoldR_ * mix;
        }

        // ---- mute geral e limitador suave ----
        const float mg = muteGain_.next();
        mL = softClip(mL * mg);
        mR = softClip(mR * mg);

        // ---- háptica (canais 3–4): kick + baixo a 150 Hz + impulsos + metrónomo ----
        float hap = hapLp2_.process(hapLp1_.process(hapIn)) * 1.6f * mg;
        for (auto& im : impulses_)
            if (im.pos >= 0) hap += impulseSample(im, hapProfile) * 0.8f;
        hap = hapOut ? std::clamp(hap * hapMax, -1.0f, 1.0f) : 0.0f;

        out[f * 4 + 0] = mL;
        out[f * 4 + 1] = mR;
        out[f * 4 + 2] = hap;
        out[f * 4 + 3] = hap;

        // ---- análise para as luzes ----
        masterEnv_.process((mL + mR) * 0.5f);
        const float low = splitLp_.process((mL + mR) * 0.5f);
        const float high = (mL + mR) * 0.5f - low;
        eLow_ += (low * low - eLow_) * 0.0005f;
        eHigh_ += (high * high - eHigh_) * 0.0005f;

        if (rec) rec->push(mL, mR);
        pos_++;
    }

    // Envelopes por pad (a cada bloco): ataque ~10 ms, release ~250 ms.
    const float blockS = static_cast<float>(frames) / 48000.0f;
    const float att = 1.0f - std::exp(-blockS / 0.010f);
    const float rel = 1.0f - std::exp(-blockS / 0.250f);
    for (int i = 0; i < kPads; ++i) {
        const float lvl = padAbs_[i] / (2.0f * static_cast<float>(std::max(frames, 1)));
        padEnv_[i] += (lvl - padEnv_[i]) * (lvl > padEnv_[i] ? att : rel);
        status.padLevel[i].store(padEnv_[i]);
        status.loopState[i].store(loops_[static_cast<size_t>(i)].state);
    }
    status.masterEnv.store(masterEnv_.value);
    status.brightness.store(eHigh_ / (eLow_ + eHigh_ + 1e-9f));
    status.beatPhase.store(static_cast<float>(std::clamp(1.0 - static_cast<double>(nextBeat_ - pos_) / fpb_, 0.0, 1.0)));
    status.dropState.store(dropState_);
    status.rewinding.store(rewindPhase_);
    status.muted.store(muted_ ? 1 : 0);
    status.restartPending.store(restartPending_ ? 1 : 0);
    status.hasPack.store(pk ? 1 : 0);
    status.framePos.store(pos_);
}

} // namespace orimbux
