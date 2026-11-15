// Mapeamento do comando para o motor de atuação, luzes e gatilhos.
// Corre na thread do SDL (é o ControllerHandler da app).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "controller.h"
#include "pack.h"

namespace orimbux {

enum PerfMode : int { kModeMenu = 0, kModePerform = 1, kModeRaioX = 2 };

// Eventos para a interface (Kotlin lê-os ~60×/s).
enum PerfEventType : int {
    kEvOpenPacks = 1,
    kEvRecToggle = 2,
    kEvMicStart = 3,   // a = pad
    kEvMicStop = 4,    // a = pad
    kEvConnected = 5,  // a = 1 ligado / 0 desligado
    kEvRewind = 6,
};

class PerfInput : public ControllerHandler {
public:
    static PerfInput& get();

    // ControllerHandler
    void onButton(int button, bool down, float strengthA0, int64_t t0MonoNs) override;
    void onHitRefined(int button, float a0, float a1) override;
    void onTick(int64_t nowMonoNs, const ControllerState& state) override;
    void onConnection(bool connected) override;

    // ---- definições (UI) ----
    std::atomic<int> mode{kModeMenu};
    std::atomic<bool> leftHanded{false};
    std::atomic<bool> dynamicsOn{true};
    std::atomic<float> dynLow{0.15f}, dynHigh{3.0f};  // força (m/s²) de um toque fraco / forte
    std::atomic<float> gyroSensitivity{1.0f};
    std::atomic<bool> invertPitch{false}, invertRoll{false};
    std::atomic<float> gyroBias[3]{};
    std::atomic<float> lightIntensity{0.8f};
    std::atomic<bool> ledInvert{false};
    std::atomic<bool> recording{false};
    std::atomic<uint32_t> padColor[kPads]{};
    void requestGyroCalibration() { calibrateGyro_.store(true); }

    // ---- estado para a interface ----
    std::atomic<float> filterValue{0};
    std::atomic<bool> filterLocked{false};
    std::atomic<int> fxType{0};
    std::atomic<float> fxAmount{0};
    std::atomic<int> rollDiv{0};
    std::atomic<float> volLoops{0.85f}, volOne{0.85f};
    std::atomic<uint32_t> padsHeld{0};
    std::atomic<bool> shiftHeld{false};
    std::atomic<uint32_t> cutsMask{0};
    std::atomic<float> lastStrength{0};
    std::atomic<int> micPad{-1};

    int popEvents(int* out, int maxPairs);  // pares (tipo, valor)

private:
    PerfInput();
    void pushEvent(int type, int value);
    int padForButton(int button) const;
    bool isRole(int button, int role) const;
    float gainFor(float strength);
    void releaseAll();
    void updateLights(int64_t now, const ControllerState& s);
    void updateTriggers(int64_t now);

    // Papéis dos botões (trocados no modo canhoto)
    enum Role { kRoleShift, kRoleFx, kRoleLock };

    std::mutex evMutex_;
    std::vector<int> events_;

    // Estado do mapeamento (só na thread do SDL)
    int lastMode_ = kModeMenu;
    bool shift_ = false, fxHold_ = false, createDown_ = false, createUsed_ = false;
    int64_t createDownAt_ = 0;
    int heldOrder_[kPads] = {};
    int heldSeq_ = 0;
    uint32_t held_ = 0;
    float lockedFilter_ = 0;
    bool gliding_ = false;
    float filterOut_ = 0;
    int rollDivState_ = 0;
    float refAccel_[3] = {0, 0, 1};
    int64_t lastRewind_ = 0;
    int64_t micStartedAt_ = 0;
    int micPad_ = -1;
    float prevTouchY_[2] = {-1, -1};
    float prevTouchX_[2] = {0, 0};
    bool prevTouchDown_[2] = {false, false};
    std::atomic<bool> calibrateGyro_{false};

    // Luzes
    float lr_ = 0, lg_ = 0, lb_ = 0;  // cor suavizada (linear)
    int lastFlash_[kPads] = {};
    int flashPad_ = -1;
    int64_t flashAt_ = 0;
    float flashAmp_ = 0;
    int lastBeatCount_ = 0;
    int64_t beatAt_ = 0;
    bool beatIsBar_ = false;
    int lastDrop_ = 0;
    int64_t dropFlashAt_ = 0;
    int64_t lastLightTick_ = 0, lastLightSend_ = 0;
    uint8_t sentR_ = 0, sentG_ = 0, sentB_ = 0, sentMask_ = 0xFF, sentMic_ = 0xFF;

    // Gatilhos
    int lastRollSteps_ = 0;
    int64_t r2OffAt_ = 0;
    int r2Mode_ = -1;   // 0 off · 1 pulso · 2 vibração contínua (1/32)
    bool l2Slope_ = false;
};

} // namespace orimbux
