package pt.orimbux.pad

/**
 * Ponte para o motor nativo (liborimbux.so): comando (SDL3) e áudio (Oboe).
 * As bibliotecas são carregadas em [SdlBootstrap].
 */
object NativeBridge {
    // Saídas de áudio
    const val PATH_PHONE = 0
    const val PATH_CTRL = 1

    // Cenários do teste de saída de som
    const val SCENARIO_NONE = 0
    const val SCENARIO_A = 1 // música no telemóvel + háptica no comando (2 streams)
    const val SCENARIO_B = 2 // música no jack do comando + háptica (1 stream de 4 canais)

    // Tons de teste
    const val TONE_CLICK = 0
    const val TONE_HAPTIC = 1
    const val TONE_SPEAKER = 2
    const val TONE_JACK = 3

    // Modos dos gatilhos (ver controller.cpp)
    const val TRIGGER_OFF = 0
    const val TRIGGER_FEEDBACK = 1
    const val TRIGGER_WEAPON = 2
    const val TRIGGER_VIBRATION = 3
    const val TRIGGER_SLOPE = 4
    const val TRIGGER_RHYTHMIC = 5
    const val TRIGGER_SIMPLE_RESISTANCE = 6 // formato simples 0x01 (o do teste do SDL3)
    const val TRIGGER_SIMPLE_VIBRATION = 7  // formato simples 0x06 (o do teste do SDL3)

    // Caminho de áudio dentro do comando
    const val ROUTE_JACK = 0
    const val ROUTE_SPEAKER = 1

    // ---- comando ----
    @JvmStatic external fun controllerStart(): Boolean
    @JvmStatic external fun controllerStop()
    /** ints[16]: ver jni_bridge.cpp · floats[24]: eixos, gyro, accel, toques, taxa de reports */
    @JvmStatic external fun controllerState(ints: IntArray, floats: FloatArray)
    @JvmStatic external fun controllerError(): String
    /** Último erro do SDL ao enviar um efeito (LEDs, gatilhos, áudio do comando). */
    @JvmStatic external fun effectError(): String
    /** Força das batidas: out[64×8] = seq, botão, a0, a1, a3, g0, g1, g3. Devolve quantas. */
    @JvmStatic external fun controllerHits(out: FloatArray): Int
    /** Mute LED como o kernel Linux faz (também liga o mute do micro). */
    @JvmStatic external fun muteLedKernel(mode: Int)
    @JvmStatic external fun rumble(low: Int, high: Int, durationMs: Int)
    @JvmStatic external fun lightbar(r: Int, g: Int, b: Int)
    @JvmStatic external fun playerLeds(mask: Int)
    @JvmStatic external fun muteLed(mode: Int)
    @JvmStatic external fun trigger(right: Boolean, mode: Int)
    @JvmStatic external fun audioRoute(route: Int, volume: Int)
    @JvmStatic external fun resetOutputs()

    // ---- áudio ----
    @JvmStatic external fun audioOpenOboe(path: Int, deviceId: Int, channels: Int, quadMask: Boolean): String
    @JvmStatic external fun audioBeginPull(path: Int, channels: Int)
    @JvmStatic external fun audioRenderPull(path: Int, buffer: FloatArray, frames: Int)
    @JvmStatic external fun audioPullTimestamp(path: Int, framePosition: Long, nanoTime: Long)
    @JvmStatic external fun audioClose(path: Int)
    @JvmStatic external fun audioCloseAll()
    @JvmStatic external fun audioSetScenario(scenario: Int)
    @JvmStatic external fun audioSetTestMusic(on: Boolean)
    @JvmStatic external fun audioTone(path: Int, tone: Int)
    /** out[7]: nº medições, mín, média, máx, última (ms), atraso até ao callback (ms), nº estimadas */
    @JvmStatic external fun audioStats(path: Int, out: FloatArray)
    @JvmStatic external fun audioClearStats()
    @JvmStatic external fun audioStatus(path: Int): String

    // ---- plano B: áudio USB direto (usbfs isócrono) ----
    /** fd com a interface 1 reclamada e na alternativa 1. Devolve "" ou o texto do erro. */
    @JvmStatic external fun directStart(fd: Int, endpoint: Int, numUrbs: Int, packetsPerUrb: Int): String
    @JvmStatic external fun directStop()
    @JvmStatic external fun directSetTone(on: Boolean)
    @JvmStatic external fun directSetPulse(on: Boolean)
    @JvmStatic external fun directClearStats()
    /** Háptica: nível máximo 0–1 (0,4 por defeito) · perfil 0 = 80 Hz suave, 1 = transiente */
    @JvmStatic external fun directSetHaptic(level: Float, profile: Int)
    /** out[14]: ver jni_bridge.cpp */
    @JvmStatic external fun directStats(out: FloatArray)
    /** Grava o micro diretamente (interface 2). Devolve amostras estéreo intercaladas (vazio = erro). */
    @JvmStatic external fun directRecord(fd: Int, endpoint: Int, seconds: Int): ShortArray
    @JvmStatic external fun directRecordError(): String
}
