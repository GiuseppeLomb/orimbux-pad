package pt.orimbux.pad.raiox

import android.content.Context
import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.audio.AudioDevices
import pt.orimbux.pad.audio.DirectUsbAudio
import pt.orimbux.pad.audio.MicTest

/** Tamanho da fila de URBs isócronos (1 pacote = 1 ms). */
enum class DirectBuffer(val label: String, val urbs: Int, val packets: Int) {
    MS4("4 ms (4×1)", 4, 1),
    MS8("8 ms (4×2)", 4, 2),
    MS16("16 ms (8×2)", 8, 2),
}

data class DirectView(
    val running: Boolean = false,
    val elapsedS: Float = 0f,
    val urbs: Long = 0,
    val packetErrors: Long = 0,
    val underruns: Long = 0,
    val submitErrors: Long = 0,
    val maxGapMs: Float = 0f,
    val latCount: Int = 0,
    val latMin: Float = 0f,
    val latAvg: Float = 0f,
    val latMax: Float = 0f,
    val lastErrno: Int = 0,
    val queueMs: Float = 0f,
) {
    fun latencyText() = if (latCount == 0) "sem medições" else
        "n=%d · mín %.1f · média %.1f · máx %.1f ms".format(latCount, latMin, latAvg, latMax)

    fun healthText() = "URBs %d · erros de pacote %d · cortes %d · erros de envio %d · maior intervalo %.1f ms%s".format(
        urbs, packetErrors, underruns, submitErrors, maxGapMs, if (lastErrno != 0) " · último errno $lastErrno" else ""
    )
}

/**
 * Prova do plano B no Raio-X: áudio USB direto para o comando, com verificações automáticas
 * (interface reclamada, latência, 5 minutos sem cortes, SDL3 em simultâneo, micro pelo Android).
 */
class DirectLab(private val ctx: Context) {
    private val usb = DirectUsbAudio(ctx)
    private val buf = FloatArray(14)

    var buffer by mutableStateOf(DirectBuffer.MS8)
    var message by mutableStateOf("")
        private set
    var view by mutableStateOf(DirectView())
        private set
    var tone by mutableStateOf(false)
        private set
    var pulse by mutableStateOf(false)
        private set
    var stabilityRun by mutableStateOf(false)
        private set
    var micBusy by mutableStateOf(false)
        private set

    private var startedAtMs = 0L
    private var micAndroidChecked = false
    private var stabilityDone = false

    fun start(model: RaioXModel) {
        // O caminho do áudio dentro do comando tem de estar no jack (auscultadores L/R).
        NativeBridge.audioRoute(NativeBridge.ROUTE_JACK, 60)
        val r = usb.startOut(buffer.urbs, buffer.packets)
        message = r
        if (r.startsWith("ERRO")) {
            model.set("direct_claim", Status.FAIL, r.removePrefix("ERRO: "))
            return
        }
        NativeBridge.directSetTone(tone)
        NativeBridge.directSetPulse(pulse)
        startedAtMs = SystemClock.elapsedRealtime()
        micAndroidChecked = false
    }

    fun stop() {
        usb.stopOut()
        stabilityRun = false
        poll(null, null)
    }

    fun setToneOn(on: Boolean) {
        tone = on
        NativeBridge.directSetTone(on)
    }

    fun setPulseOn(on: Boolean) {
        pulse = on
        NativeBridge.directSetPulse(on)
    }

    fun clear() = NativeBridge.directClearStats()

    /** Teste de 5 minutos: tom + pulso ligados, contadores a zero. */
    fun startStability(model: RaioXModel) {
        if (!usb.isRunning) start(model)
        if (!usb.isRunning) return
        setToneOn(true)
        setPulseOn(true)
        NativeBridge.directClearStats()
        stabilityRun = true
        stabilityDone = false
    }

    fun recordMic(model: RaioXModel) {
        if (micBusy) return
        micBusy = true
        Thread {
            val r = usb.recordMic(3)
            if (r.samples.isEmpty()) {
                model.set("direct_mic_usb", Status.FAIL, r.error)
            } else {
                val (peak, rms) = MicTest.measureAndPlayStereo(r.samples, AudioDevices.speaker(ctx))
                val ok = peak > -50
                model.set(
                    "direct_mic_usb", if (ok) Status.OK else Status.FAIL,
                    "pico %.0f dBFS · RMS %.0f dBFS · %d amostras estéreo".format(peak, rms, r.samples.size / 2),
                )
            }
            micBusy = false
        }.start()
    }

    /** Chamado ~5 vezes por segundo pelo ecrã. */
    fun poll(model: RaioXModel?, pad: PadSnapshot?) {
        NativeBridge.directStats(buf)
        val v = DirectView(
            running = buf[0] > 0.5f, elapsedS = buf[1], urbs = buf[2].toLong(), packetErrors = buf[3].toLong(),
            underruns = buf[4].toLong(), submitErrors = buf[5].toLong(), maxGapMs = buf[6], latCount = buf[7].toInt(),
            latMin = buf[8], latAvg = buf[9], latMax = buf[10], lastErrno = buf[12].toInt(), queueMs = buf[13],
        )
        view = v
        if (model == null || !v.running) return
        val sinceStart = SystemClock.elapsedRealtime() - startedAtMs

        // 1) Interface reclamada e áudio a sair: centenas de URBs concluídos
        if (v.urbs > 200 && model.get("direct_claim").status != Status.OK) {
            model.set("direct_claim", Status.OK, "${v.urbs} URBs isócronos enviados sem root · fila de %.0f ms".format(v.queueMs))
        }
        // 2) Latência botão→som (≥ 10 medições)
        if (v.latCount >= 10) {
            val detail = v.latencyText() + " · até sair pelo USB, com fila de %.0f ms (mais o buffer interno do comando, que não se mede)".format(v.queueMs)
            model.set("direct_latency", if (v.latAvg < 15f) Status.OK else Status.FAIL, detail)
        }
        // 3) 5 minutos seguidos sem cortes
        if (stabilityRun && !stabilityDone && v.elapsedS >= 300f) {
            stabilityDone = true
            stabilityRun = false
            val clean = v.underruns == 0L && v.packetErrors == 0L && v.submitErrors == 0L
            model.set("direct_stability", if (clean) Status.OK else Status.FAIL, "5 min · " + v.healthText())
        }
        // 4) SDL3 a funcionar ao mesmo tempo (taxa de reports do comando durante o áudio)
        if (pad != null && sinceStart > 20_000 && pad.hidapi) {
            val ok = pad.reportRateHz >= 240f
            if (model.get("direct_sdl").status == Status.PENDING || !ok) {
                model.set(
                    "direct_sdl", if (ok) Status.OK else Status.FAIL,
                    "reports a %.0f Hz durante o áudio direto · confirma também as luzes e os gatilhos".format(pad.reportRateHz),
                )
            }
        }
        // 5) O micro continua a ser visto pelo Android? (3 s depois de reclamar a interface 1)
        if (!micAndroidChecked && sinceStart > 3_000) {
            micAndroidChecked = true
            val inp = AudioDevices.controllerInput(ctx)
            model.set(
                "direct_mic_android", if (inp != null) Status.OK else Status.FAIL,
                if (inp != null) "o Android continua a ver o micro: ${AudioDevices.describe(inp)}"
                else "o Android perdeu o micro ao reclamar a interface 1 → o micro tem de ser lido diretamente (interface 2)",
            )
        }
    }
}
