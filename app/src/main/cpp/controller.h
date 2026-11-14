// Camada do comando DualSense, por cima do SDL3 (driver HIDAPI PS5).
//
// Corre numa thread própria ("thread do SDL"): inicializa o SDL, lê o comando
// ~1000 vezes por segundo, guarda um retrato do estado para a interface e executa
// os comandos de saída (luzes, gatilhos, rumble, áudio) pedidos pela app.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace orimbux {

// Índices dos botões = valores de SDL_GamepadButton (0..20) + 2 bits extra
// para L2/R2 digitais (calculados a partir do eixo analógico).
enum ButtonBit : int {
    kBtnCross = 0,      // SDL_GAMEPAD_BUTTON_SOUTH
    kBtnCircle = 1,     // EAST
    kBtnSquare = 2,     // WEST
    kBtnTriangle = 3,   // NORTH
    kBtnCreate = 4,     // BACK
    kBtnPS = 5,         // GUIDE
    kBtnOptions = 6,    // START
    kBtnL3 = 7,
    kBtnR3 = 8,
    kBtnL1 = 9,
    kBtnR1 = 10,
    kBtnUp = 11,
    kBtnDown = 12,
    kBtnLeft = 13,
    kBtnRight = 14,
    kBtnMute = 15,      // MISC1
    kBtnTouchpad = 20,  // TOUCHPAD
    kBtnL2 = 28,
    kBtnR2 = 29,
};

// Retrato do estado do comando, copiado para a interface a 60 Hz.
struct ControllerState {
    int connected = 0;
    uint32_t buttons = 0;
    float axes[6] = {};      // LX, LY, RX, RY (-1..1) · L2, R2 (0..1)
    float gyro[3] = {};      // rad/s
    float accel[3] = {};     // m/s²
    int touchDown[2] = {};
    float touchX[2] = {};    // 0..1
    float touchY[2] = {};    // 0..1
    int batteryPercent = -1;
    int powerState = 0;      // SDL_PowerState
    float sensorRateHz = 0;  // reports por segundo (via eventos do giroscópio)
    int addedCount = 0;      // nº de ligações detetadas
    int removedCount = 0;    // nº de desligações detetadas
    int sdlReady = 0;        // 1 = SDL_Init OK, -1 = falhou
    int lastEffectOk = 1;    // resultado do último envio de efeito
    int firmware = 0;
    int vendor = 0;
    int product = 0;
};

// Chamado na thread do SDL quando um botão de pad é carregado.
// t0MonoNs = momento (CLOCK_MONOTONIC) em que o SDL processou o report.
using PadPressCallback = std::function<void(int64_t t0MonoNs, int button)>;

class Controller {
public:
    static Controller& get();

    bool start();   // arranca a thread do SDL (idempotente)
    void stop();    // repõe o comando (luzes/gatilhos/rumble off) e pára

    void snapshot(ControllerState& out);
    std::string lastError();

    void setPadPressCallback(PadPressCallback cb);

    // --- Saídas (seguras de chamar de qualquer thread; executam na thread do SDL) ---
    void rumble(int low, int high, int durationMs);
    void setLightbar(int r, int g, int b);
    void setPlayerLeds(int mask);           // bits 0..4, da esquerda para a direita
    void setMuteLed(int mode);              // 0 apagado, 1 aceso, 2 a pulsar
    // mode: 0 off · 1 feedback · 2 weapon · 3 vibração · 4 rampa (L2) · 5 clique ritmado (R2)
    void setTrigger(bool right, int mode);
    // route: 0 = jack (auscultadores L/R) · 1 = altifalante do comando (canal R)
    void setAudioRoute(int route, int volume);
    void resetOutputs();

private:
    Controller() = default;
    void threadMain();
    void post(std::function<void()> fn);
    void drainCommands();
    bool sendEffect(const uint8_t* effect, int size);
    void updateSnapshot();
    void tickRhythmicTrigger(int64_t nowNs);
    void openFirstGamepad();
    void doResetOutputs();

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex cmdMutex_;
    std::vector<std::function<void()>> commands_;

    std::mutex stateMutex_;
    ControllerState state_{};
    std::string lastError_;

    std::mutex cbMutex_;
    PadPressCallback padPress_;

    void* gamepad_ = nullptr;  // SDL_Gamepad*
    uint32_t gamepadId_ = 0;   // SDL_JoystickID

    // Medição da taxa de reports (eventos do giroscópio por segundo).
    int sensorEvents_ = 0;
    int64_t sensorWindowStart_ = 0;
    float sensorRate_ = 0;

    // Teste do "clique ritmado" no R2 (1/16 a 172 BPM).
    bool rhythmicR2_ = false;
    int64_t rhythmicNext_ = 0;
    int64_t rhythmicOffAt_ = 0;
};

} // namespace orimbux
