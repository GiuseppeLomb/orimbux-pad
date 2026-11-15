// Ver perf_input.h. Mapeamento completo da especificação ("MAPEAMENTO COMPLETO").
#include "perf_input.h"

#include <SDL3/SDL.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>

#include "clock.h"
#include "perf_engine.h"
#include "trigger_effects.h"

#define LOG_TAG "OrimbuxPerf"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace orimbux {

namespace {
constexpr int64_t kMs = 1'000'000LL;
constexpr float kPiF = 3.14159265f;

// Botões (valores de SDL_GamepadButton)
constexpr int B_SOUTH = SDL_GAMEPAD_BUTTON_SOUTH, B_EAST = SDL_GAMEPAD_BUTTON_EAST, B_WEST = SDL_GAMEPAD_BUTTON_WEST,
              B_NORTH = SDL_GAMEPAD_BUTTON_NORTH, B_CREATE = SDL_GAMEPAD_BUTTON_BACK, B_PS = SDL_GAMEPAD_BUTTON_GUIDE,
              B_OPTIONS = SDL_GAMEPAD_BUTTON_START, B_L3 = SDL_GAMEPAD_BUTTON_LEFT_STICK, B_R3 = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
              B_L1 = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, B_R1 = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, B_UP = SDL_GAMEPAD_BUTTON_DPAD_UP,
              B_DOWN = SDL_GAMEPAD_BUTTON_DPAD_DOWN, B_LEFT = SDL_GAMEPAD_BUTTON_DPAD_LEFT, B_RIGHT = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
              B_MUTE = SDL_GAMEPAD_BUTTON_MISC1, B_TOUCH = SDL_GAMEPAD_BUTTON_TOUCHPAD;

float deadzone(float x, float dz) {
    const float a = std::fabs(x);
    if (a <= dz) return 0.0f;
    return std::copysign(std::min(1.0f, (a - dz) / (1.0f - dz)), x);
}

float srgbToLinear(uint8_t c) { return std::pow(static_cast<float>(c) / 255.0f, 2.2f); }
uint8_t linearToSrgb(float v) {
    return static_cast<uint8_t>(std::clamp(std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f / 2.2f) * 255.0f + 0.5f, 0.0f, 255.0f));
}
} // namespace

PerfInput& PerfInput::get() {
    static PerfInput instance;
    return instance;
}

PerfInput::PerfInput() {
    // Cores por defeito da especificação (graves escuros, agudos claros).
    const uint32_t def[kPads] = {0x8B0000, 0x3A0CA3, 0xFF6B00, 0xFFE66D, 0x2D9D5C, 0x00B4D8, 0xE0218A, 0xE8F6FF};
    for (int i = 0; i < kPads; ++i) padColor[i].store(def[i]);
}

void PerfInput::pushEvent(int type, int value) {
    std::lock_guard<std::mutex> lock(evMutex_);
    if (events_.size() < 256) {
        events_.push_back(type);
        events_.push_back(value);
    }
}

int PerfInput::popEvents(int* out, int maxPairs) {
    std::lock_guard<std::mutex> lock(evMutex_);
    const int n = std::min(maxPairs, static_cast<int>(events_.size() / 2));
    for (int i = 0; i < n * 2; ++i) out[i] = events_[static_cast<size_t>(i)];
    events_.erase(events_.begin(), events_.begin() + n * 2);
    return n;
}

// Pad de cada botão. Destro: losango ✕□○△ = pads 0–3, setas ↓←→↑ = pads 4–7.
// Canhoto: espelho horizontal (✕↔↓, □↔→, ○↔←, △↔↑).
int PerfInput::padForButton(int b) const {
    if (!leftHanded.load()) {
        switch (b) {
            case B_SOUTH: return kPadCross;
            case B_WEST: return kPadSquare;
            case B_EAST: return kPadCircle;
            case B_NORTH: return kPadTriangle;
            case B_DOWN: return kPadDown;
            case B_LEFT: return kPadLeft;
            case B_RIGHT: return kPadRight;
            case B_UP: return kPadUp;
            default: return -1;
        }
    }
    switch (b) {
        case B_DOWN: return kPadCross;
        case B_RIGHT: return kPadSquare;
        case B_LEFT: return kPadCircle;
        case B_UP: return kPadTriangle;
        case B_SOUTH: return kPadDown;
        case B_EAST: return kPadLeft;
        case B_WEST: return kPadRight;
        case B_NORTH: return kPadUp;
        default: return -1;
    }
}

bool PerfInput::isRole(int b, int role) const {
    const bool lh = leftHanded.load();
    switch (role) {
        case kRoleShift: return b == (lh ? B_R1 : B_L1);   // camada de loops
        case kRoleFx: return b == (lh ? B_L1 : B_R1);      // segurar + inclinar = efeitos
        case kRoleLock: return b == (lh ? B_R3 : B_L3);    // bloquear o filtro
        default: return false;
    }
}

// Força da batida → ganho. Curva suave e contínua em escala logarítmica (−20 dB … 0 dB).
// Os limites fraco/forte ajustam-se devagar ao uso. Sem dinâmica: ±3 dB de humanização.
float PerfInput::gainFor(float s) {
    if (!dynamicsOn.load()) {
        const float r = static_cast<float>(SDL_rand(10000)) / 10000.0f;
        return std::pow(10.0f, (r * 6.0f - 3.0f) / 20.0f);
    }
    float lo = dynLow.load(), hi = dynHigh.load();
    if (s > hi) {
        hi = hi * 0.92f + s * 0.08f;
        dynHigh.store(hi);
    } else if (s > 0.01f && s < lo) {
        lo = lo * 0.95f + s * 0.05f;
        dynLow.store(lo);
    }
    hi = std::max(hi, lo * 2.0f);
    const float x = std::clamp((std::log(std::max(s, 1e-4f)) - std::log(lo)) / (std::log(hi) - std::log(lo)), 0.0f, 1.0f);
    const float db = -20.0f + 20.0f * std::pow(x, 0.75f);
    return std::pow(10.0f, db / 20.0f);
}

void PerfInput::onConnection(bool connected) {
    pushEvent(kEvConnected, connected ? 1 : 0);
    // Ao religar, as luzes e os gatilhos têm de ser reenviados.
    sentMask_ = 0xFF;
    sentMic_ = 0xFF;
    l2Slope_ = false;
    r2Mode_ = -1;
}

void PerfInput::onButton(int b, bool down, float strengthA0, int64_t t0) {
    if (mode.load() != kModePerform) return;  // nos menus a interface trata a navegação
    PerfEngine& eng = PerfEngine::get();

    if (b == B_TOUCH) {
        if (down) eng.drop();
        return;
    }
    if (b == B_CREATE) {
        if (down) {
            createDown_ = true;
            createUsed_ = false;
            createDownAt_ = t0;
        } else {
            createDown_ = false;
            if (micPad_ >= 0) {
                pushEvent(kEvMicStop, micPad_);
                micPad_ = -1;
                micPad.store(-1);
            } else if (!createUsed_ && t0 - createDownAt_ < 500 * kMs) {
                pushEvent(kEvRecToggle, 0);  // toque: começar/parar a gravação do set
            }
        }
        return;
    }
    if (b == B_OPTIONS) {
        if (down) pushEvent(kEvOpenPacks, 0);
        return;
    }
    if (b == B_PS) {
        if (down) eng.restartLoops();
        return;
    }
    if (b == B_MUTE) {
        if (down) eng.toggleMute();
        return;
    }
    if (isRole(b, kRoleShift)) {
        shift_ = down;
        shiftHeld.store(down);
        return;
    }
    if (isRole(b, kRoleFx)) {
        fxHold_ = down;
        if (!down) eng.setFx(fxType.load(), 0.0f);  // desvanece em ~200 ms
        return;
    }
    if (isRole(b, kRoleLock)) {
        if (down) {
            const bool lock = !filterLocked.load();
            filterLocked.store(lock);
            if (lock) {
                lockedFilter_ = filterOut_;
            } else {
                gliding_ = true;  // volta suavemente a 0 (ou à posição do stick)
            }
        }
        return;
    }
    const int pad = padForButton(b);
    if (pad < 0) return;
    if (down) {
        if (createDown_) {
            // Create seguro + pad: grava pelo micro do comando para este pad.
            createUsed_ = true;
            if (micPad_ < 0) {
                micPad_ = pad;
                micPad.store(pad);
                micStartedAt_ = monoNs();
                pushEvent(kEvMicStart, pad);
            }
            return;
        }
        if (shift_) {
            eng.loopToggle(pad);  // L1 + pad: liga/desliga o loop no compasso seguinte
            return;
        }
        lastStrength.store(strengthA0);
        eng.padOn(pad, gainFor(strengthA0));
        held_ |= 1u << pad;
        heldOrder_[pad] = ++heldSeq_;
    } else {
        if (micPad_ == pad) {
            pushEvent(kEvMicStop, pad);
            micPad_ = -1;
            micPad.store(-1);
        }
        eng.padOff(pad);
        held_ &= ~(1u << pad);
    }
    padsHeld.store(held_);
}

void PerfInput::onHitRefined(int b, float a0, float a1) {
    if (mode.load() != kModePerform || !dynamicsOn.load() || shift_ || createDown_) return;
    const int pad = padForButton(b);
    if (pad < 0 || !(held_ & (1u << pad))) return;
    if (a1 > a0 * 1.15f) PerfEngine::get().padRefine(pad, gainFor(a1));
}

void PerfInput::releaseAll() {
    PerfEngine& eng = PerfEngine::get();
    for (int p = 0; p < kPads; ++p)
        if (held_ & (1u << p)) eng.padOff(p);
    held_ = 0;
    padsHeld.store(0);
    shift_ = fxHold_ = createDown_ = false;
    shiftHeld.store(false);
    eng.setRoll(0, -1);
    eng.setCuts(0);
    eng.setFx(fxType.load(), 0.0f);
    eng.setEchoOut(0.0f);
    rollDivState_ = 0;
    rollDiv.store(0);
    uint8_t off[11];
    trigger::off(off);
    Controller::get().directTrigger(false, off);
    Controller::get().directTrigger(true, off);
    l2Slope_ = false;
    r2Mode_ = 0;
    if (micPad_ >= 0) {
        pushEvent(kEvMicStop, micPad_);
        micPad_ = -1;
        micPad.store(-1);
    }
}

void PerfInput::onTick(int64_t now, const ControllerState& s) {
    const int m = mode.load();
    if (m != lastMode_) {
        if (lastMode_ == kModePerform) releaseAll();
        lastMode_ = m;
        sentMask_ = 0xFF;  // força o reenvio das luzes
    }
    if (calibrateGyro_.exchange(false)) {
        float g[3];
        Controller::get().gyroAverage(g);
        for (int i = 0; i < 3; ++i) gyroBias[i].store(g[i]);
        LOGI("Ponto neutro do giroscópio: %.3f %.3f %.3f", g[0], g[1], g[2]);
    }
    if (m == kModePerform && s.connected && s.hidapi) {
        PerfEngine& eng = PerfEngine::get();
        const bool lh = leftHanded.load();

        // ---- filtro de DJ bipolar (stick esquerdo X; zona morta 10%) ----
        const float v = deadzone(lh ? s.axes[2] : s.axes[0], 0.10f);
        float out;
        if (filterLocked.load()) {
            out = lockedFilter_;
        } else if (gliding_) {
            filterOut_ += (v - filterOut_) * 0.0033f;  // ~300 ms a 1 kHz
            if (std::fabs(filterOut_ - v) < 0.01f) gliding_ = false;
            out = filterOut_;
        } else {
            out = v;
        }
        filterOut_ = out;
        eng.setFilter(out);
        filterValue.store(out);

        // ---- cortes por grupo (stick direito, momentâneo, zona morta 30%) ----
        const float cx = lh ? s.axes[0] : s.axes[2];
        const float cy = lh ? s.axes[1] : s.axes[3];
        uint32_t mask = 0;
        if (cy < -0.3f) mask |= (1u << kPadTriangle) | (1u << kPadUp);                       // ↑ agudos
        if (cy > 0.3f) mask |= (1u << kPadSquare);                                            // ↓ baixo
        if (cx < -0.3f) mask |= (1u << kPadCross) | (1u << kPadCircle);                       // ← bateria
        if (cx > 0.3f) mask |= (1u << kPadDown) | (1u << kPadLeft) | (1u << kPadRight);       // → melodia
        eng.setCuts(mask);
        cutsMask.store(mask);

        // ---- R2: beat roll / note repeat (divisão pela pressão, com histerese) ----
        const float p = lh ? s.axes[4] : s.axes[5];
        static const float kTh[4] = {0.15f, 0.40f, 0.65f, 0.85f};
        int target = 0;
        for (int i = 3; i >= 0; --i)
            if (p >= kTh[i]) { target = i + 1; break; }
        if (target > rollDivState_) {
            rollDivState_ = target;
        } else if (target < rollDivState_) {
            const float th = kTh[rollDivState_ - 1] - 0.03f;  // só desce abaixo do limiar − 3%
            if (p < th) rollDivState_ = target;
        }
        int notePad = -1, best = -1;
        for (int i = 0; i < kPads; ++i)
            if ((held_ & (1u << i)) && heldOrder_[i] > best) { best = heldOrder_[i]; notePad = i; }
        eng.setRoll(rollDivState_, notePad);
        rollDiv.store(rollDivState_);

        // ---- L2: echo out ----
        const float e = lh ? s.axes[5] : s.axes[4];
        eng.setEchoOut(std::pow(std::clamp(e, 0.0f, 1.0f), 1.5f));

        // ---- R1 + inclinar: efeitos pelo giroscópio/acelerómetro ----
        if (fxHold_) {
            // Referência = posição do comando no momento em que se carregou no R1
            // (refAccel_ é atualizado a cada tick enquanto o R1 está solto).
            const float ax = s.accel[0], ay = s.accel[1], az = s.accel[2];
            float pitch = (std::atan2(az, ay) - std::atan2(refAccel_[2], refAccel_[1])) * 180.0f / kPiF;
            float roll = (std::atan2(ax, ay) - std::atan2(refAccel_[0], refAccel_[1])) * 180.0f / kPiF;
            if (pitch > 180) pitch -= 360;
            if (pitch < -180) pitch += 360;
            if (roll > 180) roll -= 360;
            if (roll < -180) roll += 360;
            if (invertPitch.load()) pitch = -pitch;
            if (invertRoll.load()) roll = -roll;
            const float sens = gyroSensitivity.load();
            int type;
            float angle;
            if (std::fabs(pitch) >= std::fabs(roll)) {
                type = pitch > 0 ? kFxEcho : kFxReverb;  // frente = echo · trás = reverb
                angle = std::fabs(pitch);
            } else {
                type = roll < 0 ? kFxFlanger : kFxBitcrush;  // esquerda = flanger · direita = bitcrush
                angle = std::fabs(roll);
            }
            const float amt = std::clamp((angle * sens - 5.0f) / 40.0f, 0.0f, 1.0f);  // zona morta 5°, cheio aos 45°
            eng.setFx(type, amt);
            fxType.store(type);
            fxAmount.store(amt);
        } else {
            refAccel_[0] = s.accel[0];
            refAccel_[1] = s.accel[1];
            refAccel_[2] = s.accel[2];
            fxAmount.store(0);
            // ---- golpe rápido de rotação (yaw) = REWIND (só com R1 solto, limiar alto) ----
            const float yaw = s.gyro[1] - gyroBias[1].load();
            const float thr = 9.0f / std::max(gyroSensitivity.load(), 0.2f);
            if (std::fabs(yaw) > thr && now - lastRewind_ > 1500 * kMs) {
                lastRewind_ = now;
                eng.rewind();
                pushEvent(kEvRewind, 0);
            }
        }

        // ---- touchpad: deslizar na vertical = volumes (esquerda loops, direita one-shots) ----
        float vl = volLoops.load(), vo = volOne.load();
        for (int f = 0; f < 2; ++f) {
            const bool d = s.touchDown[f] != 0;
            if (d && prevTouchDown_[f] && prevTouchY_[f] >= 0) {
                const float dy = s.touchY[f] - prevTouchY_[f];
                if (prevTouchX_[f] < 0.5f) vl = std::clamp(vl - dy * 1.3f, 0.0f, 1.0f);
                else vo = std::clamp(vo - dy * 1.3f, 0.0f, 1.0f);
            }
            prevTouchDown_[f] = d;
            prevTouchY_[f] = d ? s.touchY[f] : -1.0f;
            prevTouchX_[f] = s.touchX[f];
        }
        volLoops.store(vl);
        volOne.store(vo);
        eng.setVolumes(vl, vo);

        // ---- gravação pelo micro: máximo 4 s ----
        if (micPad_ >= 0 && now - micStartedAt_ > 4000 * kMs) {
            pushEvent(kEvMicStop, micPad_);
            micPad_ = -1;
            micPad.store(-1);
        }
        updateTriggers(now);
    }
    updateLights(now, s);
}

void PerfInput::updateTriggers(int64_t now) {
    Controller& c = Controller::get();
    const bool lh = leftHanded.load();
    const bool echoRight = lh;  // destro: echo out no L2 · canhoto: no R2
    const bool rollRight = !lh;
    uint8_t fx[11];
    if (!l2Slope_) {
        // Echo out: resistência a crescer com a pressão.
        trigger::slopeFeedback(fx, 0, 9, 1, 8);
        c.directTrigger(echoRight, fx);
        l2Slope_ = true;
    }
    const int steps = PerfEngine::get().status.rollSteps.load();
    if (rollDivState_ == 0) {
        if (r2Mode_ != 0) {
            trigger::off(fx);
            c.directTrigger(rollRight, fx);
            r2Mode_ = 0;
            r2OffAt_ = 0;
        }
    } else if (rollDivState_ == 4) {
        // 1/32: vibração contínua à frequência das repetições (um report só).
        if (r2Mode_ != 2) {
            const float bpm = PerfEngine::get().status.bpm.load();
            trigger::vibration(fx, 0, 6, static_cast<int>(std::lround(bpm / 60.0f * 8.0f)));
            c.directTrigger(rollRight, fx);
            r2Mode_ = 2;
        }
    } else {
        // 1/4 a 1/16: um clique sentido a cada repetição do roll.
        if (steps != lastRollSteps_) {
            trigger::vibration(fx, 0, 7, 180);
            c.directTrigger(rollRight, fx);
            r2OffAt_ = now + 15 * kMs;
            r2Mode_ = 1;
        } else if (r2OffAt_ != 0 && now >= r2OffAt_) {
            trigger::off(fx);
            c.directTrigger(rollRight, fx);
            r2OffAt_ = 0;
        }
    }
    lastRollSteps_ = steps;
}

void PerfInput::updateLights(int64_t now, const ControllerState& s) {
    if (now - lastLightTick_ < 16 * kMs) return;
    lastLightTick_ = now;
    const int m = mode.load();
    if (m == kModeRaioX || !s.connected || !s.hidapi) return;  // o Raio-X controla as luzes
    const auto& st = PerfEngine::get().status;
    const double t = static_cast<double>(now) / 1e9;

    // COR = mistura das cores dos instrumentos a tocar, pesada pelo volume de cada um.
    float r = 0, g = 0, b = 0, sum = 0;
    for (int i = 0; i < kPads; ++i) {
        const float w = std::pow(std::min(st.padLevel[i].load() * 6.0f, 1.0f), 0.8f);
        if (w <= 0.0005f) continue;
        const uint32_t c = padColor[i].load();
        r += srgbToLinear(static_cast<uint8_t>(c >> 16)) * w;
        g += srgbToLinear(static_cast<uint8_t>(c >> 8)) * w;
        b += srgbToLinear(static_cast<uint8_t>(c)) * w;
        sum += w;
    }
    if (sum > 1e-4f) {
        r /= sum;
        g /= sum;
        b /= sum;
    } else {
        r = g = b = 0.01f;
    }
    // TOM = claridade pelo centroide: graves → escuro e profundo; agudos → claro.
    const float tone = std::clamp(st.brightness.load() * 3.0f, 0.0f, 1.0f);
    const float dark = 0.35f + 0.65f * tone;
    const float light = std::max(0.0f, tone - 0.6f) * 0.6f;
    r = r * dark + (1.0f - r * dark) * light;
    g = g * dark + (1.0f - g * dark) * light;
    b = b * dark + (1.0f - b * dark) * light;
    // INTENSIDADE = envelope de energia do master.
    const float env = std::clamp(st.masterEnv.load() * 3.5f, 0.0f, 1.0f);
    const float k = 0.10f + 0.90f * env;
    r *= k;
    g *= k;
    b *= k;
    // ONE-SHOT = flash na cor do instrumento, ~300 ms.
    for (int i = 0; i < kPads; ++i) {
        const int fc = st.flashCount[i].load();
        if (fc != lastFlash_[i]) {
            lastFlash_[i] = fc;
            flashPad_ = i;
            flashAt_ = now;
            flashAmp_ = std::clamp(st.flashGain[i].load(), 0.2f, 1.0f);
        }
    }
    bool fast = false;
    if (flashPad_ >= 0) {
        const float e = flashAmp_ * std::exp(-static_cast<float>(now - flashAt_) / (300.0f * kMs));
        if (e < 0.01f) {
            flashPad_ = -1;
        } else {
            const uint32_t c = padColor[flashPad_].load();
            r += (srgbToLinear(static_cast<uint8_t>(c >> 16)) - r) * e;
            g += (srgbToLinear(static_cast<uint8_t>(c >> 8)) - g) * e;
            b += (srgbToLinear(static_cast<uint8_t>(c)) - b) * e;
            fast = e > 0.3f;
        }
    }
    // Estados especiais
    const int drop = st.dropState.load();
    if (drop == 2) r = g = b = 0.0f;                         // Drop: apaga…
    if (lastDrop_ == 2 && drop == 0) dropFlashAt_ = now;     // …e volta num flash no tempo 1
    lastDrop_ = drop;
    if (dropFlashAt_ != 0) {
        const float e = std::exp(-static_cast<float>(now - dropFlashAt_) / (300.0f * kMs));
        r += (1.0f - r) * e;
        g += (1.0f - g) * e;
        b += (1.0f - b) * e;
        fast = true;
        if (e < 0.01f) dropFlashAt_ = 0;
    }
    if (filterLocked.load()) {  // filtro bloqueado: respiração lenta
        const float br = 0.65f + 0.35f * static_cast<float>(std::sin(2.0 * kPiF * 0.25 * t));
        r *= br;
        g *= br;
        b *= br;
    }
    if (recording.load()) {  // a gravar: ponto vermelho a pulsar por cima
        const float pr = 0.6f * (0.5f + 0.5f * static_cast<float>(std::sin(2.0 * kPiF * 1.0 * t)));
        r += (1.0f - r) * pr;
        g *= 1.0f - pr;
        b *= 1.0f - pr;
    }
    if (st.muted.load()) {
        r *= 0.15f;
        g *= 0.15f;
        b *= 0.15f;
    }
    const float gi = std::clamp(lightIntensity.load(), 0.0f, 1.0f);
    r *= gi;
    g *= gi;
    b *= gi;
    // Transições suaves (sem saltos nem tremidos); os flashes entram mais depressa.
    const float a = fast ? 0.6f : 0.23f;
    lr_ += (r - lr_) * a;
    lg_ += (g - lg_) * a;
    lb_ += (b - lb_) * a;
    const uint8_t R = linearToSrgb(lr_), G = linearToSrgb(lg_), Bc = linearToSrgb(lb_);

    // PLAYER LEDs: frase de 16 compassos. Exteriores enchem 1 a cada 4 compassos; central pisca no tempo.
    const int bc = st.beatCount.load();
    if (bc != lastBeatCount_) {
        lastBeatCount_ = bc;
        beatAt_ = now;
        beatIsBar_ = st.beatInBar.load() == 0;
    }
    const int bar = st.barInPhrase.load();
    const int filled = std::clamp(bar / 4 + 1, 1, 4);
    static const int kOuter[4] = {0, 1, 3, 4};
    uint8_t mask = 0;
    if (st.hasPack.load()) {
        for (int i = 0; i < filled; ++i) mask |= static_cast<uint8_t>(1u << kOuter[i]);
        if (now - beatAt_ < (beatIsBar_ ? 200 : 80) * kMs) mask |= 1u << 2;
    }
    if (ledInvert.load()) {
        uint8_t inv = 0;
        for (int i = 0; i < 5; ++i)
            if (mask & (1u << i)) inv |= static_cast<uint8_t>(1u << (4 - i));
        mask = inv;
    }
    const uint8_t mic = st.muted.load() ? 1 : 0;  // mute geral = mute LED aceso

    // Um só report para todas as luzes; cor no máximo 30×/s e só quando muda.
    const bool maskChanged = mask != sentMask_ || mic != sentMic_;
    const bool colorChanged = std::abs(R - sentR_) >= 2 || std::abs(G - sentG_) >= 2 || std::abs(Bc - sentB_) >= 2;
    if (maskChanged || (colorChanged && now - lastLightSend_ >= 33 * kMs)) {
        Controller::get().directLights(R, G, Bc, mask, mic);
        sentR_ = R;
        sentG_ = G;
        sentB_ = Bc;
        sentMask_ = mask;
        sentMic_ = mic;
        lastLightSend_ = now;
    }
}

} // namespace orimbux
