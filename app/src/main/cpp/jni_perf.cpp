// Ponte JNI da app de atuação (pt.orimbux.pad.perf.PerfBridge).
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <cstdio>
#include <string>
#include <vector>

#include "controller.h"
#include "pack.h"
#include "perf_engine.h"
#include "perf_input.h"
#include "perf_output.h"
#include "usb_direct.h"
#include "wav.h"

using namespace orimbux;

#define PERF_FN(name) Java_pt_orimbux_pad_perf_PerfBridge_##name

namespace {
AAssetManager* g_assets = nullptr;
jobject g_assetsRef = nullptr;

std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out(c ? c : "");
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}

// "asset:packs/id/ficheiro.wav" → assets da APK; qualquer outro caminho → sistema de ficheiros.
bool readFile(const std::string& path, std::vector<uint8_t>& out, std::string& err) {
    if (path.rfind("asset:", 0) == 0) {
        if (!g_assets) {
            err = "assets não inicializados";
            return false;
        }
        AAsset* a = AAssetManager_open(g_assets, path.substr(6).c_str(), AASSET_MODE_BUFFER);
        if (!a) {
            err = "não encontrei " + path;
            return false;
        }
        const auto* data = static_cast<const uint8_t*>(AAsset_getBuffer(a));
        const off_t len = AAsset_getLength(a);
        out.assign(data, data + len);
        AAsset_close(a);
        return true;
    }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        err = "não consegui abrir " + path;
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(static_cast<size_t>(std::max(len, 0L)));
    const size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    out.resize(got);
    return true;
}

jstring toJ(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }
} // namespace

extern "C" {

JNIEXPORT void JNICALL PERF_FN(init)(JNIEnv* env, jclass, jobject assetManager) {
    if (g_assetsRef) env->DeleteGlobalRef(g_assetsRef);
    g_assetsRef = env->NewGlobalRef(assetManager);
    g_assets = AAssetManager_fromJava(env, g_assetsRef);
    Controller::get().setHandler(&PerfInput::get());
}

// ---------------------------------------------------------------- packs

JNIEXPORT jlong JNICALL PERF_FN(packCreate)(JNIEnv* env, jclass, jstring id, jfloat bpm) {
    auto* p = new Pack();
    p->id = jstr(env, id);
    p->bpm = bpm > 0 ? bpm : 172.0f;
    return reinterpret_cast<jlong>(p);
}

JNIEXPORT jstring JNICALL PERF_FN(packSlot)(JNIEnv* env, jclass, jlong handle, jint pad, jboolean isLoop, jstring path,
                                            jint bars, jfloat gainDb) {
    auto* p = reinterpret_cast<Pack*>(handle);
    if (!p) return toJ(env, "pack inválido");
    std::vector<uint8_t> bytes;
    std::string err;
    const std::string pth = jstr(env, path);
    if (!readFile(pth, bytes, err)) return toJ(env, err);
    StereoBuffer buf;
    if (!decodeWav(bytes.data(), bytes.size(), buf, err)) return toJ(env, pth + ": " + err);
    if (!packSetSlot(*p, pad, isLoop == JNI_TRUE, std::move(buf), bars, gainDb)) return toJ(env, pth + ": slot inválido");
    return toJ(env, "");
}

JNIEXPORT void JNICALL PERF_FN(packDemo)(JNIEnv*, jclass, jlong handle) {
    if (auto* p = reinterpret_cast<Pack*>(handle)) synthDemoPack(*p);
}

JNIEXPORT jboolean JNICALL PERF_FN(packCommit)(JNIEnv*, jclass, jlong handle, jboolean quantized) {
    auto* p = reinterpret_cast<Pack*>(handle);
    return p && PerfEngine::get().setPack(p, quantized == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL PERF_FN(packFree)(JNIEnv*, jclass, jlong handle) { delete reinterpret_cast<Pack*>(handle); }

// Som gravado pelo micro: copia o pack atual, substitui o one-shot e troca já (sem quantizar).
JNIEXPORT jstring JNICALL PERF_FN(replaceOneshot)(JNIEnv* env, jclass, jint pad, jstring path) {
    Pack* cur = PerfEngine::get().currentPack();
    if (!cur) return toJ(env, "sem pack ativo");
    std::vector<uint8_t> bytes;
    std::string err;
    if (!readFile(jstr(env, path), bytes, err)) return toJ(env, err);
    StereoBuffer buf;
    if (!decodeWav(bytes.data(), bytes.size(), buf, err)) return toJ(env, err);
    auto* copy = new Pack(*cur);
    packSetSlot(*copy, pad, false, std::move(buf), 0, 0);
    if (!PerfEngine::get().setPack(copy, false)) {
        delete copy;
        return toJ(env, "troca de pack em curso, tenta outra vez");
    }
    return toJ(env, "");
}

JNIEXPORT void JNICALL PERF_FN(collect)(JNIEnv*, jclass) { delete PerfEngine::get().takeRetiredPack(); }

JNIEXPORT jstring JNICALL PERF_FN(currentPackId)(JNIEnv* env, jclass) {
    Pack* p = PerfEngine::get().currentPack();
    return toJ(env, p ? p->id : "");
}

// ---------------------------------------------------------------- saídas

JNIEXPORT jstring JNICALL PERF_FN(startController)(JNIEnv* env, jclass, jint fd, jint urbs, jint packets) {
    return toJ(env, PerfOutput::get().startController(fd, urbs, packets));
}

JNIEXPORT jstring JNICALL PERF_FN(startPhone)(JNIEnv* env, jclass, jint deviceId, jint fd, jint urbs, jint packets,
                                              jint hapticDelayMs) {
    return toJ(env, PerfOutput::get().startPhone(deviceId, fd, urbs, packets, hapticDelayMs));
}

JNIEXPORT void JNICALL PERF_FN(stopOutput)(JNIEnv*, jclass) { PerfOutput::get().stop(); }

JNIEXPORT jint JNICALL PERF_FN(outputMode)(JNIEnv*, jclass) { return PerfOutput::get().mode(); }

// ---------------------------------------------------------------- modo e definições

JNIEXPORT void JNICALL PERF_FN(setMode)(JNIEnv*, jclass, jint mode) { PerfInput::get().mode.store(mode); }

JNIEXPORT void JNICALL PERF_FN(settings)(JNIEnv*, jclass, jboolean leftHanded, jboolean dynamics, jfloat gyroSens,
                                         jboolean invertPitch, jboolean invertRoll, jfloat lightIntensity,
                                         jboolean ledInvert, jfloat hapticMax, jboolean metroOn, jfloat metroLevel,
                                         jint hapticProfile) {
    PerfInput& in = PerfInput::get();
    in.leftHanded.store(leftHanded == JNI_TRUE);
    in.dynamicsOn.store(dynamics == JNI_TRUE);
    in.gyroSensitivity.store(gyroSens);
    in.invertPitch.store(invertPitch == JNI_TRUE);
    in.invertRoll.store(invertRoll == JNI_TRUE);
    in.lightIntensity.store(lightIntensity);
    in.ledInvert.store(ledInvert == JNI_TRUE);
    PerfEngine::get().setHaptics(hapticMax, metroOn == JNI_TRUE, metroLevel, hapticProfile);
}

JNIEXPORT void JNICALL PERF_FN(setPadGate)(JNIEnv*, jclass, jint pad, jboolean gate) {
    PerfEngine::get().setPadGate(pad, gate == JNI_TRUE);
}

JNIEXPORT void JNICALL PERF_FN(setPadColors)(JNIEnv* env, jclass, jintArray colors) {
    jint c[kPads];
    env->GetIntArrayRegion(colors, 0, std::min<jsize>(kPads, env->GetArrayLength(colors)), c);
    for (int i = 0; i < kPads; ++i) PerfInput::get().padColor[i].store(static_cast<uint32_t>(c[i]) & 0xFFFFFF);
}

JNIEXPORT void JNICALL PERF_FN(setDynRange)(JNIEnv*, jclass, jfloat lo, jfloat hi) {
    PerfInput::get().dynLow.store(lo);
    PerfInput::get().dynHigh.store(hi);
}

JNIEXPORT void JNICALL PERF_FN(getDynRange)(JNIEnv* env, jclass, jfloatArray out) {
    jfloat v[2] = {PerfInput::get().dynLow.load(), PerfInput::get().dynHigh.load()};
    env->SetFloatArrayRegion(out, 0, 2, v);
}

JNIEXPORT void JNICALL PERF_FN(calibrateGyro)(JNIEnv*, jclass) { PerfInput::get().requestGyroCalibration(); }

// ---------------------------------------------------------------- gravação do set

JNIEXPORT jboolean JNICALL PERF_FN(recStart)(JNIEnv*, jclass, jint fd) {
    return PerfOutput::get().recStart(fd) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL PERF_FN(recStop)(JNIEnv*, jclass) { return PerfOutput::get().recStop(); }

// ---------------------------------------------------------------- estado para a interface

/** ints[32] e floats[32]: ver PerfBridge.kt */
JNIEXPORT void JNICALL PERF_FN(snapshot)(JNIEnv* env, jclass, jintArray ints, jfloatArray floats) {
    const auto& st = PerfEngine::get().status;
    PerfInput& in = PerfInput::get();
    const DirectStats ds = DirectUsbOut::get().stats();
    jint iv[32] = {};
    iv[0] = st.hasPack.load();
    iv[1] = st.barInPhrase.load();
    iv[2] = st.beatInBar.load();
    iv[3] = st.dropState.load();
    iv[4] = st.rewinding.load();
    iv[5] = st.muted.load();
    iv[6] = st.restartPending.load();
    iv[7] = st.packPending.load();
    iv[8] = in.rollDiv.load();
    iv[9] = in.fxType.load();
    iv[10] = in.filterLocked.load() ? 1 : 0;
    iv[11] = in.shiftHeld.load() ? 1 : 0;
    iv[12] = static_cast<jint>(in.padsHeld.load());
    iv[13] = static_cast<jint>(in.cutsMask.load());
    iv[14] = in.micPad.load();
    iv[15] = PerfOutput::get().recording() ? 1 : 0;
    iv[16] = PerfOutput::get().mode();
    for (int i = 0; i < kPads; ++i) iv[17 + i] = st.loopState[i].load();
    iv[25] = st.beatCount.load();
    iv[26] = static_cast<jint>(ds.underruns);
    iv[27] = static_cast<jint>(ds.packetErrors);
    iv[28] = in.mode.load();
    jfloat fv[32] = {};
    fv[0] = st.bpm.load();
    fv[1] = st.beatPhase.load();
    fv[2] = in.filterValue.load();
    fv[3] = in.fxAmount.load();
    fv[4] = in.volLoops.load();
    fv[5] = in.volOne.load();
    fv[6] = st.masterEnv.load();
    for (int i = 0; i < kPads; ++i) fv[7 + i] = st.padLevel[i].load();
    fv[15] = in.lastStrength.load();
    fv[16] = ds.elapsedS;
    fv[17] = ds.latAvg;
    fv[18] = st.brightness.load();
    env->SetIntArrayRegion(ints, 0, std::min<jsize>(32, env->GetArrayLength(ints)), iv);
    env->SetFloatArrayRegion(floats, 0, std::min<jsize>(32, env->GetArrayLength(floats)), fv);
}

JNIEXPORT jint JNICALL PERF_FN(pollEvents)(JNIEnv* env, jclass, jintArray out) {
    int buf[64];
    const int n = PerfInput::get().popEvents(buf, std::min<int>(32, env->GetArrayLength(out) / 2));
    if (n > 0) env->SetIntArrayRegion(out, 0, n * 2, buf);
    return n;
}

} // extern "C"
