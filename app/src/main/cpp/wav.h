// Leitura e escrita de ficheiros WAV (PCM 16/24/32 bits inteiros ou 32 bits float).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace orimbux {

// Áudio estéreo em float, sempre a 48 kHz depois de carregado.
struct StereoBuffer {
    std::vector<float> l;
    std::vector<float> r;
    int frames() const { return static_cast<int>(l.size()); }
    bool empty() const { return l.empty(); }
};

// Descodifica um WAV que está em memória. Converte mono → estéreo e reamostra
// (linear) para 48 kHz se for preciso. Devolve false e preenche `error` se falhar.
bool decodeWav(const uint8_t* data, size_t size, StereoBuffer& out, std::string& error);

// Cabeçalho WAV PCM 16 bits estéreo a 48 kHz para `dataBytes` bytes de áudio.
std::vector<uint8_t> wavHeader16(uint32_t dataBytes, int channels = 2, int sampleRate = 48000);

} // namespace orimbux
