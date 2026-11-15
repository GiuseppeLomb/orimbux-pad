// Ver pack.h.
#include "pack.h"

#include <algorithm>
#include <cmath>

#include "dsp.h"

namespace orimbux {

double framesPerBar(double bpm) { return 48000.0 * 60.0 / bpm * 4.0; }

bool packSetSlot(Pack& pack, int pad, bool isLoop, StereoBuffer&& audio, int bars, float gainDb) {
    if (pad < 0 || pad >= kPads || audio.empty()) return false;
    SlotData& s = isLoop ? pack.loop[pad] : pack.oneshot[pad];
    s.audio = std::move(audio);
    s.gain = dsp::dbToGain(gainDb);
    s.bars = isLoop ? std::max(bars, 1) : 0;
    if (isLoop) {
        const int grid = static_cast<int>(std::lround(framesPerBar(pack.bpm) * s.bars));
        const int have = s.audio.frames();
        if (have < grid) {
            // Mais curto do que a grelha: completa com silêncio (o loop continua certo no compasso).
            s.audio.l.resize(static_cast<size_t>(grid), 0.0f);
            s.audio.r.resize(static_cast<size_t>(grid), 0.0f);
        }
        s.loopFrames = grid;
        s.tailFrames = std::clamp(have - grid, 0, kLoopXfadeFrames);
        s.audio.l.resize(static_cast<size_t>(grid + s.tailFrames));
        s.audio.r.resize(static_cast<size_t>(grid + s.tailFrames));
    } else {
        s.loopFrames = 0;
        s.tailFrames = 0;
        // Micro-fade de 2 ms no fim (anti-clique), caso o ficheiro termine a meio de uma onda.
        const int n = s.audio.frames();
        const int fade = std::min(96, n);
        for (int i = 0; i < fade; ++i) {
            const float g = static_cast<float>(i) / static_cast<float>(fade);
            s.audio.l[static_cast<size_t>(n - 1 - i)] *= g;
            s.audio.r[static_cast<size_t>(n - 1 - i)] *= g;
        }
    }
    s.loaded = true;
    return true;
}

} // namespace orimbux
