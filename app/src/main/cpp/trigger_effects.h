// Efeitos dos gatilhos adaptativos do DualSense (11 bytes por gatilho).
//
// Formato baseado no "TriggerEffectGenerator" de John "Nielk1" Klein
// (MIT License, Copyright (c) 2021-2022). Reescrito em C++ para este projeto.
//
// O curso do gatilho está dividido em 10 zonas (0 = solto, 9 = fundo).
// Força / amplitude: 1–8 (guardada como valor−1 em 3 bits por zona).
#pragma once

#include <cstdint>

namespace orimbux::trigger {

constexpr int kEffectSize = 11;

// Gatilho livre (sem resistência).
void off(uint8_t out[kEffectSize]);

// Resistência constante a partir de `position` (0–9) com `strength` (1–8).
void feedback(uint8_t out[kEffectSize], int position, int strength);

// Resistência por zona: strength[i] = 0 (livre) ou 1–8.
void multiFeedback(uint8_t out[kEffectSize], const int strength[10]);

// Rampa de resistência: cresce de startStrength (em startPos) até endStrength (em endPos).
void slopeFeedback(uint8_t out[kEffectSize], int startPos, int endPos, int startStrength, int endStrength);

// "Gatilho de arma": resistência entre startPos (2–7) e endPos (3–8) que cede de repente (clique).
void weapon(uint8_t out[kEffectSize], int startPos, int endPos, int strength);

// Vibração a partir de `position` com `amplitude` (1–8) e `frequencyHz` (1–255).
void vibration(uint8_t out[kEffectSize], int position, int amplitude, int frequencyHz);

} // namespace orimbux::trigger
