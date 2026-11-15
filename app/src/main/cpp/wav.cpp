// Ver wav.h.
#include "wav.h"

#include <algorithm>
#include <cstring>

namespace orimbux {

namespace {
uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
void wr16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>(x >> 8));
}
void wr32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>((x >> (8 * i)) & 0xFF));
}
} // namespace

bool decodeWav(const uint8_t* data, size_t size, StereoBuffer& out, std::string& error) {
    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        error = "não é um ficheiro WAV";
        return false;
    }
    int format = 0, channels = 0, rate = 0, bits = 0;
    const uint8_t* pcm = nullptr;
    size_t pcmBytes = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint8_t* ck = data + pos;
        const uint32_t len = rd32(ck + 4);
        const uint8_t* body = ck + 8;
        if (pos + 8 + len > size && std::memcmp(ck, "data", 4) != 0) break;
        if (std::memcmp(ck, "fmt ", 4) == 0 && len >= 16) {
            format = rd16(body);
            channels = rd16(body + 2);
            rate = static_cast<int>(rd32(body + 4));
            bits = rd16(body + 14);
            if (format == 0xFFFE && len >= 26) format = rd16(body + 24);  // WAVE_FORMAT_EXTENSIBLE
        } else if (std::memcmp(ck, "data", 4) == 0) {
            pcm = body;
            pcmBytes = std::min<size_t>(len, size - (pos + 8));
        }
        pos += 8 + len + (len & 1);
    }
    if (!pcm || channels <= 0 || rate <= 0) {
        error = "WAV sem dados ou sem formato";
        return false;
    }
    const bool isFloat = (format == 3);
    if (!(format == 1 || isFloat) || !(bits == 16 || bits == 24 || bits == 32)) {
        error = "formato WAV não suportado (só PCM 16/24/32 bits ou float 32)";
        return false;
    }
    const int bps = bits / 8;
    const size_t frames = pcmBytes / static_cast<size_t>(bps * channels);
    std::vector<float> l(frames), r(frames);
    for (size_t f = 0; f < frames; ++f) {
        float s[2] = {0, 0};
        for (int c = 0; c < std::min(channels, 2); ++c) {
            const uint8_t* p = pcm + (f * static_cast<size_t>(channels) + static_cast<size_t>(c)) * static_cast<size_t>(bps);
            float v = 0;
            if (isFloat) {
                std::memcpy(&v, p, 4);
            } else if (bits == 16) {
                v = static_cast<float>(static_cast<int16_t>(rd16(p))) / 32768.0f;
            } else if (bits == 24) {
                int32_t x = static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
                if (x & 0x800000) x |= ~0xFFFFFF;
                v = static_cast<float>(x) / 8388608.0f;
            } else {
                v = static_cast<float>(static_cast<int32_t>(rd32(p))) / 2147483648.0f;
            }
            s[c] = v;
        }
        l[f] = s[0];
        r[f] = channels >= 2 ? s[1] : s[0];
    }
    if (rate == 48000) {
        out.l.swap(l);
        out.r.swap(r);
        return true;
    }
    // Reamostragem linear simples (o script do PC já entrega a 48 kHz; isto é só uma rede de segurança).
    const double ratio = static_cast<double>(rate) / 48000.0;
    const size_t outFrames = static_cast<size_t>(static_cast<double>(frames) / ratio);
    out.l.resize(outFrames);
    out.r.resize(outFrames);
    for (size_t i = 0; i < outFrames; ++i) {
        const double src = static_cast<double>(i) * ratio;
        const size_t a = static_cast<size_t>(src);
        const size_t b = std::min(a + 1, frames - 1);
        const float t = static_cast<float>(src - static_cast<double>(a));
        out.l[i] = l[a] + (l[b] - l[a]) * t;
        out.r[i] = r[a] + (r[b] - r[a]) * t;
    }
    return true;
}

std::vector<uint8_t> wavHeader16(uint32_t dataBytes, int channels, int sampleRate) {
    std::vector<uint8_t> h;
    h.reserve(44);
    h.insert(h.end(), {'R', 'I', 'F', 'F'});
    wr32(h, 36 + dataBytes);
    h.insert(h.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    wr32(h, 16);
    wr16(h, 1);
    wr16(h, static_cast<uint16_t>(channels));
    wr32(h, static_cast<uint32_t>(sampleRate));
    wr32(h, static_cast<uint32_t>(sampleRate * channels * 2));
    wr16(h, static_cast<uint16_t>(channels * 2));
    wr16(h, 16);
    h.insert(h.end(), {'d', 'a', 't', 'a'});
    wr32(h, dataBytes);
    return h;
}

} // namespace orimbux
