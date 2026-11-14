cmake_minimum_required(VERSION 3.22.1)
project(orimbux LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# SDL3 (zlib) — camada do comando DualSense (driver HIDAPI PS5).
# O código fonte é descarregado por tools/fetch-deps.ps1 para third_party/SDL3.
# ---------------------------------------------------------------------------
set(SDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/SDL3")
if(NOT EXISTS "${SDL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Falta o SDL3: corre tools/fetch-deps.ps1")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL_ROOT}" "${CMAKE_BINARY_DIR}/SDL3")

# ---------------------------------------------------------------------------
# Oboe (Apache-2.0) — áudio de baixa latência (vem do Maven via prefab).
# ---------------------------------------------------------------------------
find_package(oboe REQUIRED CONFIG)

# ---------------------------------------------------------------------------
# liborimbux.so — motor nativo da app.
# ---------------------------------------------------------------------------
add_library(orimbux SHARED
    jni_bridge.cpp
    controller.cpp
    trigger_effects.cpp
    audio_engine.cpp
    usb_direct.cpp
)

target_compile_options(orimbux PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
target_link_libraries(orimbux PRIVATE SDL3::SDL3 oboe::oboe log android)
