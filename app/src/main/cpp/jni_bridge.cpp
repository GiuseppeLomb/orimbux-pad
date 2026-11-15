// Ponte JNI entre o Kotlin (pt.orimbux.pad.NativeBridge) e o motor nativo.
#include <jni.h>

#include <algorithm>
#include <string>

#include "audio_engine.h"
#include "controller.h"
#include "usb_direct.h"

using orimbux::Controller;
using orimbux::ControllerState;
using orimbux::Engine;

#define JNI_FN(name) Java_pt_orimbux_pad_NativeBridge_##name

namespace {
jstring toJ(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }
} // namespace

extern "C" {

// ---------------------------------------------------------------- comando

JNIEXPORT jboolean JNICALL JNI_FN(controllerStart)(JNIEnv*, jclass) {
    // Cada pad carregado dispara o som de teste e a medição de latência.
    Controller::get().setPadPressCallback([](int64_t t0, int) {
        Engine::get().padPressed(t0);
        orimbux::DirectUsbOut::get().padPressed(t0);
    });
    return Controller::get().start() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL JNI_FN(controllerStop)(JNIEnv*, jclass) { Controller::get().stop(); }

JNIEXPORT void JNICALL JNI_FN(controllerState)(JNIEnv* env, jclass, jintArray ints, jfloatArray floats) {
    ControllerState s;
    Controller::get().snapshot(s);
    jint iv[16] = {
        s.connected, static_cast<jint>(s.buttons), s.batteryPercent, s.powerState,
        s.touchDown[0], s.touchDown[1], s.addedCount, s.removedCount,
        s.sdlReady, s.lastEffectOk, s.firmware, s.vendor, s.product, s.hidapi, s.effectsSent, s.effectsFailed,
    };
    jfloat fv[24] = {
        s.axes[0], s.axes[1], s.axes[2], s.axes[3], s.axes[4], s.axes[5],
        s.gyro[0], s.gyro[1], s.gyro[2],
        s.accel[0], s.accel[1], s.accel[2],
        s.touchX[0], s.touchY[0], s.touchX[1], s.touchY[1],
        s.sensorRateHz, 0, 0, 0, 0, 0, 0, 0,
    };
    env->SetIntArrayRegion(ints, 0, std::min<jsize>(16, env->GetArrayLength(ints)), iv);
    env->SetFloatArrayRegion(floats, 0, std::min<jsize>(24, env->GetArrayLength(floats)), fv);
}

JNIEXPORT jstring JNICALL JNI_FN(controllerError)(JNIEnv* env, jclass) {
    return toJ(env, Controller::get().lastError());
}

JNIEXPORT jstring JNICALL JNI_FN(effectError)(JNIEnv* env, jclass) {
    ControllerState s;
    Controller::get().snapshot(s);
    return toJ(env, s.effectError);
}

/** out[64×8]: seq, botão, a0, a1, a3, g0, g1, g3 por batida (as mais recentes). Devolve quantas. */
JNIEXPORT jint JNICALL JNI_FN(controllerHits)(JNIEnv* env, jclass, jfloatArray out) {
    orimbux::HitResult h[64];
    const int max = std::min<int>(64, env->GetArrayLength(out) / 8);
    const int n = Controller::get().hits(h, max);
    std::vector<jfloat> v(static_cast<size_t>(n) * 8);
    for (int i = 0; i < n; ++i) {
        jfloat* p = v.data() + static_cast<size_t>(i) * 8;
        p[0] = static_cast<float>(h[i].seq);
        p[1] = static_cast<float>(h[i].button);
        p[2] = h[i].a0; p[3] = h[i].a1; p[4] = h[i].a3;
        p[5] = h[i].g0; p[6] = h[i].g1; p[7] = h[i].g3;
    }
    if (n > 0) env->SetFloatArrayRegion(out, 0, n * 8, v.data());
    return n;
}

JNIEXPORT void JNICALL JNI_FN(muteLedKernel)(JNIEnv*, jclass, jint mode) {
    Controller::get().setMuteLed(mode, true);
}

JNIEXPORT void JNICALL JNI_FN(rumble)(JNIEnv*, jclass, jint low, jint high, jint durationMs) {
    Controller::get().rumble(low, high, durationMs);
}

JNIEXPORT void JNICALL JNI_FN(lightbar)(JNIEnv*, jclass, jint r, jint g, jint b) {
    Controller::get().setLightbar(r, g, b);
}

JNIEXPORT void JNICALL JNI_FN(playerLeds)(JNIEnv*, jclass, jint mask) { Controller::get().setPlayerLeds(mask); }

JNIEXPORT void JNICALL JNI_FN(muteLed)(JNIEnv*, jclass, jint mode) { Controller::get().setMuteLed(mode); }

JNIEXPORT void JNICALL JNI_FN(trigger)(JNIEnv*, jclass, jboolean right, jint mode) {
    Controller::get().setTrigger(right == JNI_TRUE, mode);
}

JNIEXPORT void JNICALL JNI_FN(audioRoute)(JNIEnv*, jclass, jint route, jint volume) {
    Controller::get().setAudioRoute(route, volume);
}

JNIEXPORT void JNICALL JNI_FN(resetOutputs)(JNIEnv*, jclass) { Controller::get().resetOutputs(); }

// ---------------------------------------------------------------- áudio

JNIEXPORT jstring JNICALL JNI_FN(audioOpenOboe)(JNIEnv* env, jclass, jint path, jint deviceId, jint channels,
                                                jboolean quadMask) {
    return toJ(env, Engine::get().openOboe(path, deviceId, channels, quadMask == JNI_TRUE));
}

JNIEXPORT void JNICALL JNI_FN(audioBeginPull)(JNIEnv*, jclass, jint path, jint channels) {
    Engine::get().beginPull(path, channels);
}

JNIEXPORT void JNICALL JNI_FN(audioRenderPull)(JNIEnv* env, jclass, jint path, jfloatArray buf, jint frames) {
    auto* data = static_cast<float*>(env->GetPrimitiveArrayCritical(buf, nullptr));
    if (!data) return;
    Engine::get().renderPull(path, data, frames);
    env->ReleasePrimitiveArrayCritical(buf, data, 0);
}

JNIEXPORT void JNICALL JNI_FN(audioPullTimestamp)(JNIEnv*, jclass, jint path, jlong framePos, jlong nanos) {
    Engine::get().pullTimestamp(path, framePos, nanos);
}

JNIEXPORT void JNICALL JNI_FN(audioClose)(JNIEnv*, jclass, jint path) { Engine::get().closePath(path); }

JNIEXPORT void JNICALL JNI_FN(audioCloseAll)(JNIEnv*, jclass) { Engine::get().closeAll(); }

JNIEXPORT void JNICALL JNI_FN(audioSetScenario)(JNIEnv*, jclass, jint scenario) {
    Engine::get().setScenario(scenario);
}

JNIEXPORT void JNICALL JNI_FN(audioSetTestMusic)(JNIEnv*, jclass, jboolean on) {
    Engine::get().setTestMusic(on == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(audioTone)(JNIEnv*, jclass, jint path, jint tone) {
    Engine::get().requestTone(path, tone);
}

JNIEXPORT void JNICALL JNI_FN(audioStats)(JNIEnv* env, jclass, jint path, jfloatArray out) {
    const orimbux::LatencyStats s = Engine::get().stats(path);
    jfloat v[7] = {static_cast<float>(s.count), s.minMs, s.avgMs, s.maxMs, s.lastMs, s.cbDelayAvgMs,
                   static_cast<float>(s.estimated)};
    env->SetFloatArrayRegion(out, 0, std::min<jsize>(7, env->GetArrayLength(out)), v);
}

JNIEXPORT void JNICALL JNI_FN(audioClearStats)(JNIEnv*, jclass) { Engine::get().clearStats(); }

JNIEXPORT jstring JNICALL JNI_FN(audioStatus)(JNIEnv* env, jclass, jint path) {
    return toJ(env, Engine::get().status(path));
}

// ---------------------------------------------------------------- plano B: áudio USB direto

namespace {
std::string g_lastRecordError;
}

JNIEXPORT jstring JNICALL JNI_FN(directStart)(JNIEnv* env, jclass, jint fd, jint endpoint, jint numUrbs,
                                              jint packetsPerUrb) {
    return toJ(env, orimbux::DirectUsbOut::get().start(fd, endpoint, numUrbs, packetsPerUrb));
}

JNIEXPORT void JNICALL JNI_FN(directStop)(JNIEnv*, jclass) { orimbux::DirectUsbOut::get().stop(); }

JNIEXPORT void JNICALL JNI_FN(directSetTone)(JNIEnv*, jclass, jboolean on) {
    orimbux::DirectUsbOut::get().setTone(on == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(directSetPulse)(JNIEnv*, jclass, jboolean on) {
    orimbux::DirectUsbOut::get().setPulse(on == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(directClearStats)(JNIEnv*, jclass) { orimbux::DirectUsbOut::get().clearStats(); }

JNIEXPORT void JNICALL JNI_FN(directSetHaptic)(JNIEnv*, jclass, jfloat level, jint profile) {
    orimbux::DirectUsbOut::get().setHaptic(level, profile);
}

/** out[14]: a correr, segundos, URBs, erros de pacote, cortes, erros de envio, maior intervalo (ms),
 *  nº latências, mín, média, máx, última (ms), último errno, fila (ms) */
JNIEXPORT void JNICALL JNI_FN(directStats)(JNIEnv* env, jclass, jfloatArray out) {
    const orimbux::DirectStats s = orimbux::DirectUsbOut::get().stats();
    jfloat v[14] = {
        static_cast<float>(s.running), s.elapsedS, static_cast<float>(s.urbsDone),
        static_cast<float>(s.packetErrors), static_cast<float>(s.underruns), static_cast<float>(s.submitErrors),
        s.maxGapMs, static_cast<float>(s.latCount), s.latMin, s.latAvg, s.latMax, s.latLast,
        static_cast<float>(s.lastErrno), s.queueMs,
    };
    env->SetFloatArrayRegion(out, 0, std::min<jsize>(14, env->GetArrayLength(out)), v);
}

JNIEXPORT jshortArray JNICALL JNI_FN(directRecord)(JNIEnv* env, jclass, jint fd, jint endpoint, jint seconds) {
    g_lastRecordError.clear();
    const std::vector<int16_t> data = orimbux::directRecord(fd, endpoint, seconds, g_lastRecordError);
    jshortArray arr = env->NewShortArray(static_cast<jsize>(data.size()));
    if (!data.empty()) env->SetShortArrayRegion(arr, 0, static_cast<jsize>(data.size()), data.data());
    return arr;
}

JNIEXPORT jstring JNICALL JNI_FN(directRecordError)(JNIEnv* env, jclass) { return toJ(env, g_lastRecordError); }

} // extern "C"
