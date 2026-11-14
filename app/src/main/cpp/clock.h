// Relógio comum a todo o motor: CLOCK_MONOTONIC em nanossegundos.
// É o mesmo relógio que o Android usa em System.nanoTime() e nos timestamps
// do AAudio / AudioTrack, por isso as latências medidas são comparáveis.
#pragma once

#include <cstdint>
#include <ctime>

namespace orimbux {

inline int64_t monoNs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

} // namespace orimbux
