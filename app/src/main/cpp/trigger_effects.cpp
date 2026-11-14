// Ver trigger_effects.h (formato de Nielk1, MIT).
#include "trigger_effects.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orimbux::trigger {

namespace {
constexpr uint8_t kOff = 0x05;
constexpr uint8_t kFeedback = 0x21;
constexpr uint8_t kWeapon = 0x25;
constexpr uint8_t kVibration = 0x26;

// Escreve: zonas ativas (16 bits LE) em [1..2] e valores de 3 bits por zona (32 bits LE) em [3..6].
void packZones(uint8_t out[kEffectSize], const int value[10]) {
    uint32_t active = 0;
    uint32_t packed = 0;
    for (int i = 0; i < 10; ++i) {
        if (value[i] > 0) {
            const uint32_t v = static_cast<uint32_t>(std::clamp(value[i], 1, 8) - 1) & 0x07u;
            active |= 1u << i;
            packed |= v << (3 * i);
        }
    }
    out[1] = static_cast<uint8_t>(active & 0xFF);
    out[2] = static_cast<uint8_t>((active >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>(packed & 0xFF);
    out[4] = static_cast<uint8_t>((packed >> 8) & 0xFF);
    out[5] = static_cast<uint8_t>((packed >> 16) & 0xFF);
    out[6] = static_cast<uint8_t>((packed >> 24) & 0xFF);
}
} // namespace

void off(uint8_t out[kEffectSize]) {
    std::memset(out, 0, kEffectSize);
    out[0] = kOff;
}

void feedback(uint8_t out[kEffectSize], int position, int strength) {
    if (strength <= 0) { off(out); return; }
    int zones[10] = {};
    for (int i = std::clamp(position, 0, 9); i < 10; ++i) zones[i] = strength;
    multiFeedback(out, zones);
}

void multiFeedback(uint8_t out[kEffectSize], const int strength[10]) {
    std::memset(out, 0, kEffectSize);
    bool any = false;
    for (int i = 0; i < 10; ++i) any = any || strength[i] > 0;
    if (!any) { off(out); return; }
    out[0] = kFeedback;
    packZones(out, strength);
}

void slopeFeedback(uint8_t out[kEffectSize], int startPos, int endPos, int startStrength, int endStrength) {
    startPos = std::clamp(startPos, 0, 8);
    endPos = std::clamp(endPos, startPos + 1, 9);
    startStrength = std::clamp(startStrength, 1, 8);
    endStrength = std::clamp(endStrength, 1, 8);
    const float slope = static_cast<float>(endStrength - startStrength) / static_cast<float>(endPos - startPos);
    int zones[10] = {};
    for (int i = startPos; i < 10; ++i) {
        zones[i] = (i <= endPos)
            ? static_cast<int>(std::lround(startStrength + slope * static_cast<float>(i - startPos)))
            : endStrength;
    }
    multiFeedback(out, zones);
}

void weapon(uint8_t out[kEffectSize], int startPos, int endPos, int strength) {
    if (strength <= 0) { off(out); return; }
    startPos = std::clamp(startPos, 2, 7);
    endPos = std::clamp(endPos, startPos + 1, 8);
    std::memset(out, 0, kEffectSize);
    const uint16_t zones = static_cast<uint16_t>((1u << startPos) | (1u << endPos));
    out[0] = kWeapon;
    out[1] = static_cast<uint8_t>(zones & 0xFF);
    out[2] = static_cast<uint8_t>((zones >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>(std::clamp(strength, 1, 8) - 1);
}

void vibration(uint8_t out[kEffectSize], int position, int amplitude, int frequencyHz) {
    if (amplitude <= 0 || frequencyHz <= 0) { off(out); return; }
    std::memset(out, 0, kEffectSize);
    int zones[10] = {};
    for (int i = std::clamp(position, 0, 9); i < 10; ++i) zones[i] = amplitude;
    out[0] = kVibration;
    packZones(out, zones);
    out[9] = static_cast<uint8_t>(std::clamp(frequencyHz, 1, 255));
}

} // namespace orimbux::trigger
