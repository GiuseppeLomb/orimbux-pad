package pt.orimbux.pad.perf

import android.content.res.AssetManager

/** Ponte para o motor de atuação nativo (jni_perf.cpp). */
object PerfBridge {
    // Modos do comando
    const val MODE_MENU = 0
    const val MODE_PERFORM = 1
    const val MODE_RAIOX = 2

    // Saídas
    const val OUT_NONE = 0
    const val OUT_CONTROLLER = 1
    const val OUT_PHONE = 2

    // Eventos vindos do comando
    const val EV_OPEN_PACKS = 1
    const val EV_REC_TOGGLE = 2
    const val EV_MIC_START = 3
    const val EV_MIC_STOP = 4
    const val EV_CONNECTED = 5
    const val EV_REWIND = 6

    // Estado dos loops
    const val LOOP_OFF = 0
    const val LOOP_PENDING_ON = 1
    const val LOOP_ON = 2
    const val LOOP_PENDING_OFF = 3

    @JvmStatic external fun init(assets: AssetManager)

    // Packs
    @JvmStatic external fun packCreate(id: String, bpm: Float): Long
    /** path: "asset:packs/<id>/<ficheiro>.wav" ou caminho de ficheiro. Devolve "" ou o erro. */
    @JvmStatic external fun packSlot(handle: Long, pad: Int, isLoop: Boolean, path: String, bars: Int, gainDb: Float): String
    @JvmStatic external fun packDemo(handle: Long)
    /** quantized = troca no tempo 1 seguinte. false = ainda há uma troca pendente (tenta depois). */
    @JvmStatic external fun packCommit(handle: Long, quantized: Boolean): Boolean
    @JvmStatic external fun packFree(handle: Long)
    @JvmStatic external fun replaceOneshot(pad: Int, path: String): String
    @JvmStatic external fun collect()
    @JvmStatic external fun currentPackId(): String

    // Saídas
    @JvmStatic external fun startController(fd: Int, urbs: Int, packets: Int): String
    @JvmStatic external fun startPhone(deviceId: Int, fd: Int, urbs: Int, packets: Int, hapticDelayMs: Int): String
    @JvmStatic external fun stopOutput()
    @JvmStatic external fun outputMode(): Int

    // Modo e definições
    @JvmStatic external fun setMode(mode: Int)
    @JvmStatic external fun settings(
        leftHanded: Boolean, dynamics: Boolean, gyroSens: Float, invertPitch: Boolean, invertRoll: Boolean,
        lightIntensity: Float, ledInvert: Boolean, hapticMax: Float, metroOn: Boolean, metroLevel: Float, hapticProfile: Int,
    )
    @JvmStatic external fun setPadGate(pad: Int, gate: Boolean)
    @JvmStatic external fun setPadColors(colors: IntArray)
    @JvmStatic external fun setDynRange(lo: Float, hi: Float)
    @JvmStatic external fun getDynRange(out: FloatArray)
    @JvmStatic external fun calibrateGyro()

    // Gravação do set (o fd passa a ser do nativo)
    @JvmStatic external fun recStart(fd: Int): Boolean
    @JvmStatic external fun recStop(): Float

    /**
     * ints[32]: 0 tem pack · 1 compasso na frase · 2 tempo no compasso · 3 drop (0/1 pendente/2 ativo) ·
     * 4 rewind · 5 mute · 6 recomeço pendente · 7 troca de pack pendente · 8 divisão do roll · 9 efeito ·
     * 10 filtro bloqueado · 11 L1 seguro · 12 pads seguros (bits) · 13 cortes (bits) · 14 pad a gravar (micro) ·
     * 15 a gravar o set · 16 saída · 17–24 estado dos loops · 25 nº de tempos · 26 cortes USB · 27 erros USB · 28 modo
     * floats[32]: 0 BPM · 1 fase do tempo · 2 filtro · 3 quantidade do efeito · 4 vol. loops · 5 vol. one-shots ·
     * 6 envelope · 7–14 nível de cada pad · 15 última força · 16 tempo do USB (s) · 17 latência média USB · 18 claridade
     */
    @JvmStatic external fun snapshot(ints: IntArray, floats: FloatArray)
    /** Pares (tipo, valor). Devolve quantos pares. */
    @JvmStatic external fun pollEvents(out: IntArray): Int
}
