// Blocos de DSP do motor (só cabeçalho, sem alocações no áudio).
// Filtros biquad RBJ (Audio EQ Cookbook, domínio público), linhas de atraso,
// reverb estilo Freeverb (algoritmo de Jezar, domínio público), flanger e bitcrush.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace orimbux::dsp {

constexpr float kSr = 48000.0f;
constexpr float kPi = 3.14159265358979f;

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

// Suavizador de primeira ordem para parâmetros (evita "zipper noise" e cliques).
struct Smoother {
    float value = 0, target = 0, coef = 0.001f;
    void setTime(float seconds) { coef = 1.0f - std::exp(-1.0f / (std::max(seconds, 1e-4f) * kSr)); }
    void reset(float v) { value = target = v; }
    float next() {
        value += (target - value) * coef;
        return value;
    }
};

// Envelope seguidor com ataque/relaxamento separados.
struct Envelope {
    float value = 0, att = 0, rel = 0;
    void set(float attackS, float releaseS) {
        att = 1.0f - std::exp(-1.0f / (attackS * kSr));
        rel = 1.0f - std::exp(-1.0f / (releaseS * kSr));
    }
    float process(float x) {
        const float a = std::fabs(x);
        value += (a - value) * (a > value ? att : rel);
        return value;
    }
};

// Biquad RBJ (forma direta transposta II), estéreo.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = {0, 0}, z2[2] = {0, 0};

    void lowpass(float fc, float q) { design(fc, q, true); }
    void highpass(float fc, float q) { design(fc, q, false); }

    void design(float fc, float q, bool low) {
        fc = std::clamp(fc, 10.0f, kSr * 0.45f);
        const float w0 = 2.0f * kPi * fc / kSr;
        const float cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha;
        if (low) {
            b0 = (1.0f - cw) * 0.5f / a0;
            b1 = (1.0f - cw) / a0;
            b2 = b0;
        } else {
            b0 = (1.0f + cw) * 0.5f / a0;
            b1 = -(1.0f + cw) / a0;
            b2 = b0;
        }
        a1 = -2.0f * cw / a0;
        a2 = (1.0f - alpha) / a0;
    }
    float process(float x, int ch) {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

// Filtro passa-baixo de primeira ordem (usado para a háptica e para a análise).
struct OnePoleLP {
    float a = 0, z = 0;
    void set(float fc) { a = 1.0f - std::exp(-2.0f * kPi * fc / kSr); }
    float process(float x) {
        z += a * (x - z);
        return z;
    }
};

// Linha de atraso estéreo com leitura inteira.
struct DelayLine {
    std::vector<float> l, r;
    int w = 0;
    void init(int maxFrames) {
        l.assign(static_cast<size_t>(maxFrames), 0.0f);
        r.assign(static_cast<size_t>(maxFrames), 0.0f);
        w = 0;
    }
    int size() const { return static_cast<int>(l.size()); }
    void read(int delay, float& outL, float& outR) const {
        int idx = w - delay;
        while (idx < 0) idx += size();
        outL = l[static_cast<size_t>(idx)];
        outR = r[static_cast<size_t>(idx)];
    }
    // Leitura fracionária (linear) — para o flanger.
    void readFrac(float delay, float& outL, float& outR) const {
        const int d0 = static_cast<int>(delay);
        const float t = delay - static_cast<float>(d0);
        float l0, r0, l1, r1;
        read(d0, l0, r0);
        read(d0 + 1, l1, r1);
        outL = l0 + (l1 - l0) * t;
        outR = r0 + (r1 - r0) * t;
    }
    void write(float inL, float inR) {
        l[static_cast<size_t>(w)] = inL;
        r[static_cast<size_t>(w)] = inR;
        if (++w >= size()) w = 0;
    }
    void clear() {
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
    }
};

// Reverb estilo Freeverb: 8 combs + 4 allpass por canal (afinação de Jezar, ajustada a 48 kHz).
class Reverb {
public:
    void init() {
        static const int combT[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int apT[4] = {556, 441, 341, 225};
        const float s = kSr / 44100.0f;
        for (int c = 0; c < 2; ++c) {
            const int spread = c == 0 ? 0 : 23;
            for (int i = 0; i < 8; ++i) comb_[c][i].init(static_cast<int>((combT[i] + spread) * s));
            for (int i = 0; i < 4; ++i) ap_[c][i].init(static_cast<int>((apT[i] + spread) * s));
        }
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float in = (inL + inR) * 0.015f;
        float acc[2] = {0, 0};
        for (int c = 0; c < 2; ++c) {
            for (auto& cb : comb_[c]) acc[c] += cb.process(in, 0.84f, 0.2f);
            for (auto& a : ap_[c]) acc[c] = a.process(acc[c]);
        }
        outL = acc[0];
        outR = acc[1];
    }
    void clear() {
        for (auto& ch : comb_)
            for (auto& cb : ch) cb.clear();
        for (auto& ch : ap_)
            for (auto& a : ch) a.clear();
    }

private:
    struct Comb {
        std::vector<float> buf;
        int i = 0;
        float store = 0;
        void init(int n) { buf.assign(static_cast<size_t>(std::max(n, 1)), 0.0f); i = 0; store = 0; }
        float process(float x, float feedback, float damp) {
            const float y = buf[static_cast<size_t>(i)];
            store = y * (1.0f - damp) + store * damp;
            buf[static_cast<size_t>(i)] = x + store * feedback;
            if (++i >= static_cast<int>(buf.size())) i = 0;
            return y;
        }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); store = 0; }
    };
    struct Allpass {
        std::vector<float> buf;
        int i = 0;
        void init(int n) { buf.assign(static_cast<size_t>(std::max(n, 1)), 0.0f); i = 0; }
        float process(float x) {
            const float b = buf[static_cast<size_t>(i)];
            const float y = -x + b;
            buf[static_cast<size_t>(i)] = x + b * 0.5f;
            if (++i >= static_cast<int>(buf.size())) i = 0;
            return y;
        }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
    };
    Comb comb_[2][8];
    Allpass ap_[2][4];
};

} // namespace orimbux::dsp
