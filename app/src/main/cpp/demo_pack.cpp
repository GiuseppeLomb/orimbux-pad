// "Pack de teste" sintetizado por código: 172 BPM, Fá menor, 8 one-shots + 8 loops.
// Serve para a app funcionar logo, mesmo antes de haver packs reais (que não vão para o git).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "dsp.h"
#include "pack.h"

namespace orimbux {

namespace {
constexpr float SR = 48000.0f;
constexpr float kTwoPi = 6.2831853f;
using Mono = std::vector<float>;

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed) {}
    float next() {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(static_cast<int32_t>(s)) / 2147483648.0f;
    }
};

int sec(float s) { return static_cast<int>(s * SR); }
float saw(float phase) { return 2.0f * (phase - std::floor(phase)) - 1.0f; }
float sq(float phase) { return (phase - std::floor(phase)) < 0.5f ? 1.0f : -1.0f; }

// Ressonador de 2 polos (passa-banda) para formantes e varrimentos.
struct Reson {
    float b1 = 0, b2 = 0, y1 = 0, y2 = 0, g = 1;
    void set(float f, float bw) {
        const float r = std::exp(-dsp::kPi * bw / SR);
        b1 = 2.0f * r * std::cos(kTwoPi * f / SR);
        b2 = -r * r;
        g = (1.0f - r);
    }
    float process(float x) {
        const float y = g * x + b1 * y1 + b2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
};

void normalize(Mono& m, float peak) {
    float mx = 0;
    for (float v : m) mx = std::max(mx, std::fabs(v));
    if (mx > 1e-6f)
        for (float& v : m) v *= peak / mx;
}

// ---------------- one-shots ----------------
Mono kick() {
    Mono m(static_cast<size_t>(sec(0.45f)));
    float ph = 0;
    Rng rng(1);
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        const float f = 48.0f + 120.0f * std::exp(-t / 0.035f);
        ph += f / SR;
        float v = std::sin(kTwoPi * ph) * std::exp(-t / 0.22f);
        if (t < 0.003f) v += rng.next() * 0.4f * (1.0f - t / 0.003f);
        m[i] = std::tanh(v * 1.6f);
    }
    normalize(m, 0.95f);
    return m;
}

Mono sub() {
    Mono m(static_cast<size_t>(sec(1.0f)));
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        const float env = std::min(1.0f, t / 0.008f) * std::exp(-t / 0.55f);
        m[i] = std::tanh(1.4f * std::sin(kTwoPi * 43.65f * t)) * env;
    }
    normalize(m, 0.9f);
    return m;
}

Mono snare() {
    Mono m(static_cast<size_t>(sec(0.35f)));
    Rng rng(2);
    float hp = 0, prev = 0;
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        const float n = rng.next();
        hp = 0.85f * (hp + n - prev);
        prev = n;
        m[i] = 0.55f * std::sin(kTwoPi * 190.0f * t) * std::exp(-t / 0.05f) + 0.8f * hp * std::exp(-t / 0.11f);
    }
    normalize(m, 0.9f);
    return m;
}

Mono noiseHit(float len, float decay, float brightness, uint32_t seed) {
    Mono m(static_cast<size_t>(sec(len)));
    Rng rng(seed);
    float hp = 0, prev = 0, lp = 0;
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        const float n = rng.next();
        hp = 0.95f * (hp + n - prev);
        prev = n;
        lp += brightness * (hp - lp);
        m[i] = lp * std::exp(-t / decay);
    }
    normalize(m, 0.8f);
    return m;
}

Mono chord(const std::vector<float>& freqs, float len, float attack, float decay, float cutoff, uint32_t seed) {
    Mono m(static_cast<size_t>(sec(len)));
    std::vector<float> ph(freqs.size() * 2, 0.0f);
    Rng rng(seed);
    for (float& p : ph) p = (rng.next() + 1.0f) * 0.5f;
    dsp::OnePoleLP lp1, lp2;
    lp1.set(cutoff);
    lp2.set(cutoff);
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        float v = 0;
        for (size_t k = 0; k < freqs.size(); ++k) {
            ph[2 * k] += freqs[k] * 1.004f / SR;
            ph[2 * k + 1] += freqs[k] * 0.996f / SR;
            v += saw(ph[2 * k]) + saw(ph[2 * k + 1]);
        }
        const float env = std::min(1.0f, t / attack) * std::exp(-t / decay);
        const float tail = std::min(1.0f, (len - t) / 0.01f);
        m[i] = lp2.process(lp1.process(v)) * env * tail;
    }
    normalize(m, 0.7f);
    return m;
}

Mono pluck(float f, float len, uint32_t seed) {
    Mono m(static_cast<size_t>(sec(len)));
    float ph = static_cast<float>(seed % 7) / 7.0f;
    dsp::OnePoleLP lp;
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        lp.set(300.0f + 5000.0f * std::exp(-t / 0.06f));
        ph += f / SR;
        m[i] = lp.process(sq(ph) * 0.6f + saw(ph * 2.0f) * 0.3f) * std::exp(-t / 0.12f) * std::min(1.0f, (len - t) / 0.01f);
    }
    normalize(m, 0.75f);
    return m;
}

// "Ah"/"oh" de formantes: trem de impulsos glotal através de 3 ressonadores.
Mono vocal(float f0, bool oh, float len) {
    Mono m(static_cast<size_t>(sec(len)));
    Reson r1, r2, r3;
    if (oh) {
        r1.set(500, 80); r2.set(900, 100); r3.set(2400, 140);
    } else {
        r1.set(750, 80); r2.set(1220, 100); r3.set(2600, 140);
    }
    float ph = 0;
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        const float vib = 1.0f + 0.01f * std::sin(kTwoPi * 5.5f * t);
        ph += f0 * vib / SR;
        const float src = saw(ph);
        const float v = r1.process(src) * 1.0f + r2.process(src) * 0.6f + r3.process(src) * 0.3f;
        const float env = std::min(1.0f, t / 0.01f) * std::min(1.0f, (len - t) / 0.04f);
        m[i] = v * env;
    }
    normalize(m, 0.75f);
    return m;
}

Mono riser() {
    Mono m(static_cast<size_t>(sec(2.0f)));
    Rng rng(9);
    Reson bp;
    float ph = 0;
    for (size_t i = 0; i < m.size(); ++i) {
        const float t = static_cast<float>(i) / SR;
        const float x = t / 2.0f;
        bp.set(300.0f * std::pow(20.0f, x), 400.0f);
        ph += (200.0f + 1000.0f * x * x) / SR;
        const float v = bp.process(rng.next()) * 3.0f + 0.25f * std::sin(kTwoPi * ph);
        m[i] = v * x * x * std::min(1.0f, (2.0f - t) / 0.02f);
    }
    normalize(m, 0.8f);
    return m;
}

StereoBuffer toStereo(const Mono& m) {
    StereoBuffer b;
    b.l = m;
    b.r = m;
    return b;
}

// ---------------- loops ----------------
// Buffer de loop: grelha L + cauda de 5 ms. Os eventos põem-se nas iterações 0 e 1,
// para a cauda conter a continuação natural (usada no crossfade).
struct LoopBuf {
    StereoBuffer b;
    int L = 0;
    LoopBuf(int bars, float bpm) {
        L = static_cast<int>(std::lround(framesPerBar(bpm) * bars));
        b.l.assign(static_cast<size_t>(L + kLoopXfadeFrames), 0.0f);
        b.r.assign(static_cast<size_t>(L + kLoopXfadeFrames), 0.0f);
    }
    void place(const Mono& m, double atFrame, float gain, float pan = 0.0f) {
        const float gl = gain * std::min(1.0f, 1.0f - pan);
        const float gr = gain * std::min(1.0f, 1.0f + pan);
        for (int it = 0; it < 2; ++it) {
            const long start = std::lround(atFrame) + static_cast<long>(it) * L;
            for (size_t i = 0; i < m.size(); ++i) {
                const long p = start + static_cast<long>(i);
                if (p < 0) continue;
                if (p >= static_cast<long>(b.l.size())) break;
                b.l[static_cast<size_t>(p)] += m[i] * gl;
                b.r[static_cast<size_t>(p)] += m[i] * gr;
            }
        }
    }
    void finish(float peak) {
        float mx = 0;
        for (size_t i = 0; i < b.l.size(); ++i) mx = std::max({mx, std::fabs(b.l[i]), std::fabs(b.r[i])});
        if (mx > 1e-6f) {
            const float g = peak / mx;
            for (size_t i = 0; i < b.l.size(); ++i) {
                b.l[i] *= g;
                b.r[i] *= g;
            }
        }
    }
};

const std::vector<float> kFm = {174.61f, 207.65f, 261.63f, 311.13f};
const std::vector<float> kDb = {138.59f, 174.61f, 207.65f, 261.63f};
const std::vector<float> kAb = {207.65f, 261.63f, 311.13f, 392.00f};
const std::vector<float> kEb = {155.56f, 196.00f, 233.08f, 293.66f};
const float kRoots[4] = {87.31f, 69.30f, 51.91f, 77.78f};  // F2, Db2, Ab1, Eb2

} // namespace

void synthDemoPack(Pack& pack) {
    pack.id = "demo";
    pack.bpm = 172.0f;
    const float bpm = pack.bpm;
    const double beat = 48000.0 * 60.0 / bpm;
    const double s16 = beat / 4.0;

    // One-shots
    packSetSlot(pack, kPadCross, false, toStereo(kick()), 0, 0);
    packSetSlot(pack, kPadSquare, false, toStereo(sub()), 0, 0);
    packSetSlot(pack, kPadCircle, false, toStereo(snare()), 0, 0);
    packSetSlot(pack, kPadTriangle, false, toStereo(noiseHit(1.6f, 0.6f, 0.6f, 4)), 0, -3);
    packSetSlot(pack, kPadDown, false, toStereo(chord(kFm, 0.6f, 0.005f, 0.25f, 2500, 5)), 0, -2);
    packSetSlot(pack, kPadLeft, false, toStereo(pluck(523.25f, 0.35f, 6)), 0, -2);
    packSetSlot(pack, kPadRight, false, toStereo(vocal(196.0f, false, 0.32f)), 0, -1);
    packSetSlot(pack, kPadUp, false, toStereo(riser()), 0, -2);

    const Mono k = kick(), s = snare(), hatC = noiseHit(0.05f, 0.015f, 0.9f, 11), hatO = noiseHit(0.25f, 0.09f, 0.8f, 12);
    const Mono shaker = noiseHit(0.06f, 0.02f, 0.5f, 13), rim = pluck(1200.0f, 0.04f, 14), ride = noiseHit(0.6f, 0.25f, 0.95f, 15);

    // ✕ loop: break de 2 compassos (kick 1 e 3½, snare 2 e 4, hats a colcheias)
    {
        LoopBuf lb(2, bpm);
        for (int bar = 0; bar < 2; ++bar) {
            const double b0 = bar * 4 * beat;
            lb.place(k, b0, 0.9f);
            lb.place(k, b0 + 2.5 * beat, 0.8f);
            if (bar == 1) lb.place(k, b0 + 2.75 * beat, 0.55f);
            lb.place(s, b0 + 1 * beat, 0.85f);
            lb.place(s, b0 + 3 * beat, 0.85f);
            lb.place(s, b0 + 3.75 * beat, 0.25f);  // ghost
            for (int e = 0; e < 8; ++e) lb.place(hatC, b0 + e * beat / 2, e % 2 ? 0.3f : 0.45f, 0.2f);
        }
        lb.finish(0.9f);
        packSetSlot(pack, kPadCross, true, std::move(lb.b), 2, 0);
    }
    // □ loop: baixo reese de 4 compassos (2 dentes de serra desafinados + sub)
    {
        LoopBuf lb(4, bpm);
        for (int bar = 0; bar < 4; ++bar) {
            const float f = kRoots[bar];
            const float len = static_cast<float>(4 * beat / SR) - 0.02f;
            Mono m(static_cast<size_t>(sec(len)));
            float p1 = 0, p2 = 0.3f, p3 = 0;
            dsp::OnePoleLP lp1, lp2;
            for (size_t i = 0; i < m.size(); ++i) {
                const float t = static_cast<float>(i) / SR;
                lp1.set(250.0f + 250.0f * (0.5f + 0.5f * std::sin(kTwoPi * 0.7f * t)));
                lp2.set(500.0f);
                p1 += f * 1.006f / SR;
                p2 += f * 0.994f / SR;
                p3 += f * 0.5f / SR;
                const float v = lp2.process(lp1.process(saw(p1) + saw(p2))) * 0.8f + std::sin(kTwoPi * p3) * 0.6f;
                m[i] = std::tanh(v * 1.5f) * std::min(1.0f, t / 0.01f) * std::min(1.0f, (len - t) / 0.02f);
            }
            lb.place(m, bar * 4 * beat, 0.9f);
        }
        lb.finish(0.85f);
        packSetSlot(pack, kPadSquare, true, std::move(lb.b), 4, 0);
    }
    // ○ loop: percussão (shaker a semicolcheias + rim nos contratempos)
    {
        LoopBuf lb(2, bpm);
        for (int i = 0; i < 32; ++i) {
            lb.place(shaker, i * s16, (i % 4 == 2) ? 0.5f : 0.25f, -0.3f);
            if (i % 8 == 3 || i % 8 == 6) lb.place(rim, i * s16, 0.4f, 0.3f);
        }
        lb.finish(0.7f);
        packSetSlot(pack, kPadCircle, true, std::move(lb.b), 2, -2);
    }
    // △ loop: tops (hats a semicolcheias com acentos + ride nos tempos + hat aberto)
    {
        LoopBuf lb(2, bpm);
        for (int i = 0; i < 32; ++i) {
            lb.place(hatC, i * s16, (i % 2) ? 0.22f : 0.38f, 0.25f);
            if (i % 4 == 0) lb.place(ride, i * s16, 0.25f, -0.25f);
            if (i % 8 == 6) lb.place(hatO, i * s16, 0.3f, 0.1f);
        }
        lb.finish(0.7f);
        packSetSlot(pack, kPadTriangle, true, std::move(lb.b), 2, -2);
    }
    // ↓ loop: pads (Fm – Db – Ab – Eb, um acorde por compasso, ataque lento)
    {
        LoopBuf lb(4, bpm);
        const std::vector<float>* chords[4] = {&kFm, &kDb, &kAb, &kEb};
        for (int bar = 0; bar < 4; ++bar) {
            const float len = static_cast<float>(4 * beat / SR) + 0.25f;
            lb.place(chord(*chords[bar], len, 0.35f, 8.0f, 1200.0f, 20 + static_cast<uint32_t>(bar)), bar * 4 * beat, 0.6f);
        }
        lb.finish(0.7f);
        packSetSlot(pack, kPadDown, true, std::move(lb.b), 4, -2);
    }
    // ← loop: lead (arpejo a semicolcheias das notas do acorde)
    {
        LoopBuf lb(4, bpm);
        const std::vector<float>* chords[4] = {&kFm, &kDb, &kAb, &kEb};
        for (int bar = 0; bar < 4; ++bar) {
            for (int i = 0; i < 16; ++i) {
                const int order[8] = {0, 2, 1, 3, 2, 1, 3, 2};
                const float f = (*chords[bar])[static_cast<size_t>(order[i % 8])] * 2.0f;
                lb.place(pluck(f, 0.16f, static_cast<uint32_t>(bar * 16 + i)), bar * 4 * beat + i * s16, (i % 4 == 0) ? 0.55f : 0.4f,
                         (i % 2) ? 0.3f : -0.3f);
            }
        }
        lb.finish(0.7f);
        packSetSlot(pack, kPadLeft, true, std::move(lb.b), 4, -3);
    }
    // → loop: vocal (chops "ah"/"oh" sincopados)
    {
        LoopBuf lb(2, bpm);
        const int hits[6] = {0, 3, 6, 10, 12, 14};
        for (int bar = 0; bar < 2; ++bar) {
            for (int h = 0; h < 6; ++h) {
                const float f0 = (h % 3 == 2) ? 233.08f : 196.0f;
                lb.place(vocal(f0, h % 2 == 1, 0.14f), bar * 4 * beat + hits[h] * s16, 0.6f);
            }
        }
        lb.finish(0.7f);
        packSetSlot(pack, kPadRight, true, std::move(lb.b), 2, -2);
    }
    // ↑ loop: atmosfera (vento filtrado + drone em Fá com quinta)
    {
        LoopBuf lb(4, bpm);
        const int L = lb.L + kLoopXfadeFrames;
        Rng rng(77);
        Reson bp;
        for (int i = 0; i < L; ++i) {
            // t continua na cauda (é a continuação natural que o crossfade junta ao início)
            const float t = static_cast<float>(i) / SR;
            const float x = static_cast<float>(i % lb.L) / static_cast<float>(lb.L);
            bp.set(600.0f + 500.0f * std::sin(kTwoPi * x), 300.0f);
            const float wind = bp.process(rng.next()) * 2.0f;
            const float drone = 0.25f * std::sin(kTwoPi * 87.31f * t) + 0.15f * std::sin(kTwoPi * 130.81f * t);
            lb.b.l[static_cast<size_t>(i)] = wind * 0.8f + drone;
            lb.b.r[static_cast<size_t>(i)] = wind * 0.6f + drone;
        }
        lb.finish(0.55f);
        packSetSlot(pack, kPadUp, true, std::move(lb.b), 4, -4);
    }
}

} // namespace orimbux
