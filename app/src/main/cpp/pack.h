// Packs de som em memória: 8 pads × (one-shot + loop).
#pragma once

#include <memory>
#include <string>

#include "wav.h"

namespace orimbux {

constexpr int kPads = 8;
constexpr int kLoopXfadeFrames = 240;  // 5 ms de crossfade no ponto de loop

// Ordem dos pads (igual à tabela da especificação):
//   0 ✕ kick/break · 1 □ sub/baixo · 2 ○ snare/percussão · 3 △ crash/tops
//   4 ↓ stab de acordes/pads · 5 ← stab de synth/lead · 6 → vocal · 7 ↑ riser/atmosfera
enum PadIndex : int {
    kPadCross = 0, kPadSquare = 1, kPadCircle = 2, kPadTriangle = 3,
    kPadDown = 4, kPadLeft = 5, kPadRight = 6, kPadUp = 7,
};

struct SlotData {
    StereoBuffer audio;
    bool loaded = false;
    int bars = 0;        // loops: número de compassos
    int loopFrames = 0;  // loops: comprimento exato na grelha (sem a cauda)
    int tailFrames = 0;  // loops: frames para lá do fim, usados no crossfade
    float gain = 1.0f;
};

struct Pack {
    std::string id;
    float bpm = 172.0f;
    SlotData oneshot[kPads];
    SlotData loop[kPads];
};

// Frames de um compasso (4 tempos) a este BPM, a 48 kHz.
double framesPerBar(double bpm);

// Coloca um WAV descodificado num slot. Para loops, fixa o comprimento na grelha
// (bars × compasso) e guarda até 5 ms de cauda para o crossfade.
bool packSetSlot(Pack& pack, int pad, bool isLoop, StereoBuffer&& audio, int bars, float gainDb);

// Gera o "pack de teste" por síntese (172 BPM, 16 sons). Ver demo_pack.cpp.
void synthDemoPack(Pack& pack);

} // namespace orimbux
