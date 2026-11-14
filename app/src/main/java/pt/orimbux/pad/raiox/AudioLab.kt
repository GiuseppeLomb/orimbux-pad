package pt.orimbux.pad.raiox

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.audio.AudioDevices
import pt.orimbux.pad.audio.MicTest
import pt.orimbux.pad.audio.PullQuadTrack

/** Como se abre o stream de 4 canais para o comando (Plano A, três variantes). */
enum class CtrlMethod(val label: String) {
    OBOE_COUNT("Oboe · 4 canais"),
    OBOE_QUAD("Oboe · máscara Quad"),
    AUDIOTRACK("AudioTrack · máscara 0xF"),
}

data class LatencyView(
    val count: Int = 0,
    val min: Float = 0f,
    val avg: Float = 0f,
    val max: Float = 0f,
    val last: Float = 0f,
    val cbDelay: Float = 0f,
    val estimated: Int = 0,
) {
    fun text(): String = if (count == 0) "sem medições" else
        "n=%d · mín %.1f · média %.1f · máx %.1f ms (até ao callback: %.1f ms)%s".format(
            count, min, avg, max, cbDelay, if (estimated > 0) " · $estimated estimadas sem timestamp" else ""
        )
}

/**
 * Laboratório de áudio do Raio-X: abre os cenários A/B, os tons de teste,
 * o teste do microfone e lê as latências medidas pelo motor nativo.
 */
class AudioLab(private val ctx: Context) {
    var method by mutableStateOf(CtrlMethod.OBOE_COUNT)
    var scenario by mutableStateOf(NativeBridge.SCENARIO_NONE)
        private set
    var phoneStatus by mutableStateOf("fechado")
        private set
    var ctrlStatus by mutableStateOf("fechado")
        private set
    var openMessage by mutableStateOf("")
        private set
    var testMusic by mutableStateOf(false)
        private set
    var phoneLatency by mutableStateOf(LatencyView())
        private set
    var ctrlLatency by mutableStateOf(LatencyView())
        private set
    var charging by mutableStateOf(false)
        private set
    var hub by mutableStateOf(false)
    var speakerInfo by mutableStateOf("")
        private set
    var ctrlOutInfo by mutableStateOf("")
        private set
    var ctrlInInfo by mutableStateOf("")
        private set
    var ctrlSupportsFour by mutableStateOf(false)
        private set
    var micBusy by mutableStateOf(false)
        private set
    var micResult by mutableStateOf<MicTest.Result?>(null)
        private set

    private val pull = PullQuadTrack()
    private val statsBuf = FloatArray(7)

    fun refreshDevices() {
        val sp = AudioDevices.speaker(ctx)
        val out = AudioDevices.controllerOutput(ctx)
        val inp = AudioDevices.controllerInput(ctx)
        speakerInfo = AudioDevices.describe(sp)
        ctrlOutInfo = AudioDevices.describe(out)
        ctrlInInfo = AudioDevices.describe(inp)
        ctrlSupportsFour = AudioDevices.supportsFourChannels(out)
    }

    /** Abre o stream do comando com o método escolhido. */
    private fun openCtrl(): String {
        val dev = AudioDevices.controllerOutput(ctx) ?: return "ERRO: saída USB do comando não encontrada"
        pull.stop()
        return when (method) {
            CtrlMethod.OBOE_COUNT -> NativeBridge.audioOpenOboe(NativeBridge.PATH_CTRL, dev.id, 4, false)
            CtrlMethod.OBOE_QUAD -> NativeBridge.audioOpenOboe(NativeBridge.PATH_CTRL, dev.id, 4, true)
            CtrlMethod.AUDIOTRACK -> pull.start(dev, 4)
        }
    }

    private fun openPhone(): String {
        val sp = AudioDevices.speaker(ctx) ?: return "ERRO: altifalantes não encontrados"
        return NativeBridge.audioOpenOboe(NativeBridge.PATH_PHONE, sp.id, 2, false)
    }

    fun openScenario(s: Int) {
        closeAll()
        refreshDevices()
        NativeBridge.audioSetScenario(s)
        scenario = s
        val msg = StringBuilder()
        when (s) {
            NativeBridge.SCENARIO_A -> {
                msg.append("Telemóvel: ").append(openPhone()).append('\n')
                msg.append("Comando: ").append(openCtrl())
            }
            NativeBridge.SCENARIO_B -> {
                NativeBridge.audioRoute(NativeBridge.ROUTE_JACK, 60)
                msg.append("Comando: ").append(openCtrl())
            }
        }
        NativeBridge.audioSetTestMusic(testMusic)
        NativeBridge.audioClearStats()
        openMessage = msg.toString()
        pollNow()
    }

    /** Garante o stream do comando aberto (para os tons de teste fora de um cenário). */
    private fun ensureCtrlOpen() {
        val st = NativeBridge.audioStatus(NativeBridge.PATH_CTRL)
        if (st.startsWith("fechado") || st.startsWith("DESLIGADO") || st.startsWith("ERRO")) {
            openMessage = "Comando: " + openCtrl()
        }
    }

    fun setMusic(on: Boolean) {
        testMusic = on
        NativeBridge.audioSetTestMusic(on)
    }

    fun clearLatency() = NativeBridge.audioClearStats()

    fun hapticTone() {
        ensureCtrlOpen()
        NativeBridge.audioTone(NativeBridge.PATH_CTRL, NativeBridge.TONE_HAPTIC)
    }

    fun jackTone() {
        ensureCtrlOpen()
        NativeBridge.audioRoute(NativeBridge.ROUTE_JACK, 60)
        NativeBridge.audioTone(NativeBridge.PATH_CTRL, NativeBridge.TONE_JACK)
    }

    fun speakerTone() {
        ensureCtrlOpen()
        NativeBridge.audioRoute(NativeBridge.ROUTE_SPEAKER, 100)
        NativeBridge.audioTone(NativeBridge.PATH_CTRL, NativeBridge.TONE_SPEAKER)
    }

    /** Volta a pôr o som do comando no jack (o altifalante não é usado pela app final). */
    fun routeBackToJack() = NativeBridge.audioRoute(NativeBridge.ROUTE_JACK, 60)

    fun runMicTest(onDone: (MicTest.Result) -> Unit) {
        if (micBusy) return
        micBusy = true
        Thread {
            val r = MicTest.recordAndPlay(AudioDevices.controllerInput(ctx), AudioDevices.speaker(ctx), 3)
            micResult = r
            micBusy = false
            onDone(r)
        }.start()
    }

    fun closeAll() {
        pull.stop()
        NativeBridge.audioCloseAll()
        NativeBridge.audioSetScenario(NativeBridge.SCENARIO_NONE)
        scenario = NativeBridge.SCENARIO_NONE
        pollNow()
    }

    fun pollNow() {
        phoneStatus = NativeBridge.audioStatus(NativeBridge.PATH_PHONE)
        ctrlStatus = NativeBridge.audioStatus(NativeBridge.PATH_CTRL)
        phoneLatency = readStats(NativeBridge.PATH_PHONE)
        ctrlLatency = readStats(NativeBridge.PATH_CTRL)
        val battery = ctx.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        charging = (battery?.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) ?: 0) != 0
    }

    private fun readStats(path: Int): LatencyView {
        NativeBridge.audioStats(path, statsBuf)
        return LatencyView(
            statsBuf[0].toInt(), statsBuf[1], statsBuf[2], statsBuf[3], statsBuf[4], statsBuf[5], statsBuf[6].toInt()
        )
    }

    /** Texto do resultado de um cenário para o relatório. */
    fun scenarioSummary(): String {
        val m = if (scenario == NativeBridge.SCENARIO_NONE) "" else " · método: ${method.label}"
        val parts = mutableListOf<String>()
        if (scenario == NativeBridge.SCENARIO_A) parts += "telemóvel: ${phoneLatency.text()}"
        parts += "comando: ${ctrlLatency.text()}"
        parts += "a carregar: ${if (charging) "sim" else "não"}"
        return parts.joinToString(" | ") + m
    }
}
