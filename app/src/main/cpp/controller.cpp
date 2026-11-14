// Ver controller.h.
#include "controller.h"

#include <SDL3/SDL.h>
#include <android/log.h>

#include <chrono>
#include <cmath>
#include <cstring>

#include "clock.h"
#include "trigger_effects.h"

#define LOG_TAG "OrimbuxCtrl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace orimbux {

namespace {

// Bloco de efeitos do output report USB 0x02 do DualSense (47 bytes, sem o ID).
// Mesmo layout que DS5EffectsState_t do SDL3 / kernel Linux (ver docs/01-PESQUISA.md).
struct DS5Effects {
    uint8_t enableBits1;         // 0
    uint8_t enableBits2;         // 1
    uint8_t rumbleRight;         // 2
    uint8_t rumbleLeft;          // 3
    uint8_t headphoneVolume;     // 4
    uint8_t speakerVolume;       // 5
    uint8_t micVolume;           // 6
    uint8_t audioRoute;          // 7  (bits 4–5: caminho de saída)
    uint8_t micLightMode;        // 8
    uint8_t powerSave;           // 9
    uint8_t rightTrigger[11];    // 10
    uint8_t leftTrigger[11];     // 21
    uint8_t unknown1[5];         // 32
    uint8_t audioControl2;       // 37 (ganho do pré-amp do altifalante)
    uint8_t enableBits3;         // 38
    uint8_t unknown2[2];         // 39
    uint8_t ledAnim;             // 41
    uint8_t ledBrightness;       // 42
    uint8_t padLights;           // 43
    uint8_t ledRed;              // 44
    uint8_t ledGreen;            // 45
    uint8_t ledBlue;             // 46
};
static_assert(sizeof(DS5Effects) == 47, "layout do DS5Effects errado");

// Bits de enableBits1
constexpr uint8_t kEn1RightTrigger = 0x04;
constexpr uint8_t kEn1LeftTrigger = 0x08;
constexpr uint8_t kEn1HeadphoneVol = 0x10;
constexpr uint8_t kEn1SpeakerVol = 0x20;
constexpr uint8_t kEn1AudioRoute = 0x80;
// Bits de enableBits2
constexpr uint8_t kEn2MicLight = 0x01;
constexpr uint8_t kEn2PadLights = 0x10;
constexpr uint8_t kEn2AudioControl2 = 0x80;

// Teste "clique ritmado" do R2: 1/16 a 172 BPM.
constexpr double kRhythmicBpm = 172.0;
constexpr int64_t kRhythmicStepNs = static_cast<int64_t>(60.0 / kRhythmicBpm / 4.0 * 1e9);
constexpr int64_t kRhythmicPulseNs = 25'000'000; // 25 ms de vibração por passo

SDL_Gamepad* gp(void* p) { return static_cast<SDL_Gamepad*>(p); }

} // namespace

Controller& Controller::get() {
    static Controller instance;
    return instance;
}

bool Controller::start() {
    if (running_) return true;
    if (thread_.joinable()) thread_.join();  // thread anterior que terminou sozinha (ex.: SDL_Init falhou)
    running_ = true;
    thread_ = std::thread([this] { threadMain(); });
    return true;
}

void Controller::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void Controller::snapshot(ControllerState& out) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    out = state_;
}

std::string Controller::lastError() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return lastError_;
}

void Controller::setPadPressCallback(PadPressCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    padPress_ = std::move(cb);
}

void Controller::post(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(cmdMutex_);
    commands_.push_back(std::move(fn));
}

void Controller::drainCommands() {
    std::vector<std::function<void()>> todo;
    {
        std::lock_guard<std::mutex> lock(cmdMutex_);
        todo.swap(commands_);
    }
    for (auto& fn : todo) fn();
}

bool Controller::sendEffect(const uint8_t* effect, int size) {
    if (!gamepad_) return false;
    const bool ok = SDL_SendGamepadEffect(gp(gamepad_), effect, size);
    if (!ok) LOGW("SDL_SendGamepadEffect falhou: %s", SDL_GetError());
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_.lastEffectOk = ok ? 1 : 0;
    return ok;
}

// ---------------------------------------------------------------------------
// Saídas
// ---------------------------------------------------------------------------

void Controller::rumble(int low, int high, int durationMs) {
    post([this, low, high, durationMs] {
        if (!gamepad_) return;
        const bool ok = SDL_RumbleGamepad(gp(gamepad_), static_cast<Uint16>(low), static_cast<Uint16>(high),
                                          static_cast<Uint32>(durationMs));
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.lastEffectOk = ok ? 1 : 0;
    });
}

void Controller::setLightbar(int r, int g, int b) {
    post([this, r, g, b] {
        if (!gamepad_) return;
        const bool ok = SDL_SetGamepadLED(gp(gamepad_), static_cast<Uint8>(r), static_cast<Uint8>(g),
                                          static_cast<Uint8>(b));
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.lastEffectOk = ok ? 1 : 0;
    });
}

void Controller::setPlayerLeds(int mask) {
    post([this, mask] {
        DS5Effects e{};
        e.enableBits2 = kEn2PadLights;
        e.padLights = static_cast<uint8_t>((mask & 0x1F) | 0x20);  // 0x20 = mudar sem fade
        sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
    });
}

void Controller::setMuteLed(int mode) {
    post([this, mode] {
        DS5Effects e{};
        e.enableBits2 = kEn2MicLight;
        e.micLightMode = static_cast<uint8_t>(mode);  // 0 apagado · 1 aceso · 2 a pulsar
        sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
    });
}

void Controller::setTrigger(bool right, int mode) {
    post([this, right, mode] {
        uint8_t fx[trigger::kEffectSize];
        if (right) rhythmicR2_ = false;
        switch (mode) {
            case 1: trigger::feedback(fx, 3, 6); break;           // resistência firme a partir de 1/3
            case 2: trigger::weapon(fx, 2, 6, 8); break;          // clique de "arma"
            case 3: trigger::vibration(fx, 0, 8, 30); break;      // vibração 30 Hz
            case 4: trigger::slopeFeedback(fx, 0, 9, 1, 8); break; // rampa: resistência cresce com a pressão
            case 5:                                                // clique ritmado (só R2)
                trigger::off(fx);
                if (right) {
                    rhythmicR2_ = true;
                    rhythmicNext_ = monoNs();
                    rhythmicOffAt_ = 0;
                }
                break;
            default: trigger::off(fx); break;
        }
        DS5Effects e{};
        if (right) {
            e.enableBits1 = kEn1RightTrigger;
            std::memcpy(e.rightTrigger, fx, sizeof(fx));
        } else {
            e.enableBits1 = kEn1LeftTrigger;
            std::memcpy(e.leftTrigger, fx, sizeof(fx));
        }
        sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
    });
}

void Controller::setAudioRoute(int route, int volume) {
    post([this, route, volume] {
        DS5Effects e{};
        if (route == 1) {
            // Altifalante do comando: toca o canal R (canal 2 do stream USB).
            e.enableBits1 = kEn1AudioRoute | kEn1SpeakerVol;
            e.speakerVolume = static_cast<uint8_t>(volume > 0 ? volume : 100);
            e.audioRoute = 0x30;
            e.enableBits2 = kEn2AudioControl2;
            e.audioControl2 = 0x02;  // pré-amp +6 dB
        } else {
            // Jack 3,5 mm do comando: auscultadores L/R (canais 1–2), altifalante mudo.
            e.enableBits1 = kEn1AudioRoute | kEn1HeadphoneVol;
            e.headphoneVolume = static_cast<uint8_t>(volume > 0 ? volume : 60);
            e.audioRoute = 0x00;
        }
        sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
    });
}

void Controller::resetOutputs() {
    post([this] { doResetOutputs(); });
}

// Repõe o comando num estado neutro: luzes, gatilhos, rumble e mute LED desligados.
void Controller::doResetOutputs() {
    if (!gamepad_) return;
    rhythmicR2_ = false;
    SDL_RumbleGamepad(gp(gamepad_), 0, 0, 0);
    SDL_SetGamepadLED(gp(gamepad_), 0, 0, 0);
    DS5Effects e{};
    e.enableBits1 = kEn1RightTrigger | kEn1LeftTrigger;
    trigger::off(e.rightTrigger);
    trigger::off(e.leftTrigger);
    e.enableBits2 = kEn2MicLight | kEn2PadLights;
    e.micLightMode = 0;
    e.padLights = 0x20;
    sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
}

void Controller::tickRhythmicTrigger(int64_t nowNs) {
    if (!rhythmicR2_ || !gamepad_) return;
    if (rhythmicOffAt_ != 0 && nowNs >= rhythmicOffAt_) {
        DS5Effects e{};
        e.enableBits1 = kEn1RightTrigger;
        trigger::off(e.rightTrigger);
        sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
        rhythmicOffAt_ = 0;
    }
    if (nowNs >= rhythmicNext_) {
        DS5Effects e{};
        e.enableBits1 = kEn1RightTrigger;
        trigger::vibration(e.rightTrigger, 0, 8, 120);
        sendEffect(reinterpret_cast<uint8_t*>(&e), sizeof(e));
        rhythmicOffAt_ = nowNs + kRhythmicPulseNs;
        rhythmicNext_ += kRhythmicStepNs;
        if (rhythmicNext_ < nowNs) rhythmicNext_ = nowNs + kRhythmicStepNs;  // atrasou-se: ressincroniza
    }
}

// ---------------------------------------------------------------------------
// Thread do SDL
// ---------------------------------------------------------------------------

// O comando é "HIDAPI" quando o SDL o abre diretamente por USB (caminho que dá
// luzes, gatilhos, touchpad e sensores). Nesse caso o caminho é /dev/bus/usb/...
static bool isHidapiGamepad(SDL_JoystickID id) {
    const char* path = SDL_GetGamepadPathForID(id);
    return path && std::strncmp(path, "/dev/bus/usb", 12) == 0;
}

// Abre o comando. Ao ligar o cabo, o Android expõe-o primeiro pelo caminho dele
// (sem luzes nem gatilhos) até o SDL ter a permissão USB; quando o comando HIDAPI
// aparece, troca-se para ele. Só as ligações HIDAPI contam para o teste de religação.
void Controller::openFirstGamepad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return;
    SDL_JoystickID best = 0;
    bool bestHidapi = false;
    for (int i = 0; i < count; ++i) {
        const bool h = isHidapiGamepad(ids[i]);
        if (best == 0 || (h && !bestHidapi)) {
            best = ids[i];
            bestHidapi = h;
        }
    }
    SDL_free(ids);
    if (best == 0) return;
    if (gamepad_ && (gamepadIsHidapi_ || !bestHidapi)) return;  // já temos o melhor possível

    if (gamepad_) {  // troca do caminho do Android para o HIDAPI (não conta como desligar)
        SDL_CloseGamepad(gp(gamepad_));
        gamepad_ = nullptr;
    }
    SDL_Gamepad* g = SDL_OpenGamepad(best);
    if (!g) return;
    gamepad_ = g;
    gamepadId_ = best;
    gamepadIsHidapi_ = bestHidapi;
    SDL_SetGamepadSensorEnabled(g, SDL_SENSOR_GYRO, true);
    SDL_SetGamepadSensorEnabled(g, SDL_SENSOR_ACCEL, true);
    LOGI("Comando aberto: %s (%04x:%04x) fw=%u hidapi=%d", SDL_GetGamepadName(g), SDL_GetGamepadVendor(g),
         SDL_GetGamepadProduct(g), SDL_GetGamepadFirmwareVersion(g), bestHidapi ? 1 : 0);
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (bestHidapi) state_.addedCount += 1;
    state_.hidapi = bestHidapi ? 1 : 0;
    state_.firmware = SDL_GetGamepadFirmwareVersion(g);
    state_.vendor = SDL_GetGamepadVendor(g);
    state_.product = SDL_GetGamepadProduct(g);
}

void Controller::updateSnapshot() {
    ControllerState s;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        s = state_;
    }
    s.connected = gamepad_ != nullptr ? 1 : 0;
    s.sensorRateHz = sensorRate_;
    if (gamepad_) {
        SDL_Gamepad* g = gp(gamepad_);
        uint32_t b = 0;
        for (int i = 0; i <= SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
            if (SDL_GetGamepadButton(g, static_cast<SDL_GamepadButton>(i))) b |= 1u << i;
        }
        auto axis = [g](SDL_GamepadAxis a) { return static_cast<float>(SDL_GetGamepadAxis(g, a)); };
        s.axes[0] = axis(SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        s.axes[1] = axis(SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        s.axes[2] = axis(SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        s.axes[3] = axis(SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
        s.axes[4] = axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
        s.axes[5] = axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
        if (s.axes[4] > 0.05f) b |= 1u << kBtnL2;
        if (s.axes[5] > 0.05f) b |= 1u << kBtnR2;
        s.buttons = b;
        SDL_GetGamepadSensorData(g, SDL_SENSOR_GYRO, s.gyro, 3);
        SDL_GetGamepadSensorData(g, SDL_SENSOR_ACCEL, s.accel, 3);
        for (int f = 0; f < 2; ++f) {
            bool down = false;
            float x = 0, y = 0, p = 0;
            if (SDL_GetGamepadTouchpadFinger(g, 0, f, &down, &x, &y, &p)) {
                s.touchDown[f] = down ? 1 : 0;
                s.touchX[f] = x;
                s.touchY[f] = y;
            }
        }
        int pct = -1;
        s.powerState = static_cast<int>(SDL_GetGamepadPowerInfo(g, &pct));
        s.batteryPercent = pct;
    } else {
        s.buttons = 0;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    // Mantém os contadores que outras partes atualizam diretamente.
    s.addedCount = state_.addedCount;
    s.removedCount = state_.removedCount;
    s.lastEffectOk = state_.lastEffectOk;
    s.firmware = state_.firmware;
    s.vendor = state_.vendor;
    s.product = state_.product;
    s.sdlReady = state_.sdlReady;
    s.hidapi = state_.hidapi;
    state_ = s;
}

void Controller::threadMain() {
    // O SDL não deve mexer nos player LEDs sozinho: a app controla-os.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        const char* err = SDL_GetError();
        LOGW("SDL_Init falhou: %s", err);
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.sdlReady = -1;
        lastError_ = std::string("SDL_Init falhou: ") + err;
        running_ = false;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.sdlReady = 1;
    }
    LOGI("SDL %d.%d.%d iniciado", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    sensorWindowStart_ = monoNs();

    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_GAMEPAD_ADDED:
                    openFirstGamepad();
                    break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    if (gamepad_ && ev.gdevice.which == gamepadId_) {
                        SDL_CloseGamepad(gp(gamepad_));
                        gamepad_ = nullptr;
                        gamepadId_ = 0;
                        rhythmicR2_ = false;
                        LOGI("Comando desligado (hidapi=%d)", gamepadIsHidapi_ ? 1 : 0);
                        {
                            std::lock_guard<std::mutex> lock(stateMutex_);
                            if (gamepadIsHidapi_) state_.removedCount += 1;
                            state_.hidapi = 0;
                        }
                        gamepadIsHidapi_ = false;
                        openFirstGamepad();  // pode haver outro caminho para o mesmo comando
                    }
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                    const int b = ev.gbutton.button;
                    const bool isPad = (b >= SDL_GAMEPAD_BUTTON_SOUTH && b <= SDL_GAMEPAD_BUTTON_NORTH) ||
                                       (b >= SDL_GAMEPAD_BUTTON_DPAD_UP && b <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                    if (isPad) {
                        // Converte o timestamp do SDL (quando o report foi processado) para CLOCK_MONOTONIC.
                        const int64_t nowMono = monoNs();
                        const int64_t ageNs = static_cast<int64_t>(SDL_GetTicksNS()) - static_cast<int64_t>(ev.gbutton.timestamp);
                        const int64_t t0 = nowMono - (ageNs > 0 ? ageNs : 0);
                        PadPressCallback cb;
                        {
                            std::lock_guard<std::mutex> lock(cbMutex_);
                            cb = padPress_;
                        }
                        if (cb) cb(t0, b);
                    }
                    break;
                }
                case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
                    if (ev.gsensor.sensor == SDL_SENSOR_GYRO) sensorEvents_++;
                    break;
                default:
                    break;
            }
        }

        const int64_t now = monoNs();
        if (now - sensorWindowStart_ >= 1'000'000'000LL) {
            sensorRate_ = static_cast<float>(sensorEvents_) * 1e9f / static_cast<float>(now - sensorWindowStart_);
            sensorEvents_ = 0;
            sensorWindowStart_ = now;
        }

        drainCommands();
        tickRhythmicTrigger(now);
        updateSnapshot();

        // ~1 kHz: o comando envia reports a ~250 Hz; isto acrescenta no máximo 1 ms.
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }

    // Ao sair da app: comando reposto (luzes, gatilhos, rumble em off).
    drainCommands();
    doResetOutputs();
    SDL_Delay(30);  // dá tempo ao SDL de enviar o último report
    if (gamepad_) {
        SDL_CloseGamepad(gp(gamepad_));
        gamepad_ = nullptr;
    }
    SDL_Quit();
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_ = ControllerState{};
    LOGI("SDL terminado");
}

} // namespace orimbux
