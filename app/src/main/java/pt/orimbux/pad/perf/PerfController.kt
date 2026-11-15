package pt.orimbux.pad.perf

import android.annotation.SuppressLint
import android.content.ContentValues
import android.content.Context
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.net.Uri
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.audio.AudioDevices
import pt.orimbux.pad.audio.DirectUsbAudio
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/** Retrato do motor para o ecrã (60×/s). */
data class PerfSnapshot(
    val hasPack: Boolean = false,
    val barInPhrase: Int = 0,
    val beatInBar: Int = 0,
    val dropState: Int = 0,
    val rewinding: Boolean = false,
    val muted: Boolean = false,
    val restartPending: Boolean = false,
    val packPending: Boolean = false,
    val rollDiv: Int = 0,
    val fxType: Int = 0,
    val filterLocked: Boolean = false,
    val shiftHeld: Boolean = false,
    val padsHeld: Int = 0,
    val cuts: Int = 0,
    val micPad: Int = -1,
    val recording: Boolean = false,
    val output: Int = 0,
    val loopState: IntArray = IntArray(8),
    val beatCount: Int = 0,
    val usbUnderruns: Int = 0,
    val usbErrors: Int = 0,
    val bpm: Float = 172f,
    val beatPhase: Float = 0f,
    val filter: Float = 0f,
    val fxAmount: Float = 0f,
    val volLoops: Float = 0.85f,
    val volOne: Float = 0.85f,
    val padLevel: FloatArray = FloatArray(8),
    val lastStrength: Float = 0f,
)

/**
 * Orquestra a app de atuação: saída de som, packs, eventos do comando,
 * gravação do set (Música/OrimbuxPad/) e gravação pelo micro do comando.
 */
class PerfController(private val ctx: Context, val settings: AppSettings) {
    private val tag = "OrimbuxApp"
    private val usb = DirectUsbAudio(ctx)
    private val io = Executors.newSingleThreadExecutor()

    var snapshot by mutableStateOf(PerfSnapshot())
        private set
    var packs by mutableStateOf<List<PackInfo>>(emptyList())
        private set
    var currentPack by mutableStateOf<PackInfo?>(null)
        private set
    var loadingPack by mutableStateOf<String?>(null)
        private set
    var banner by mutableStateOf<String?>(null)
    var bannerAction by mutableStateOf<Pair<String, () -> Unit>?>(null)
    var outputInfo by mutableStateOf("")
        private set
    var openPacksRequest by mutableStateOf(0)
        private set

    private val ints = IntArray(32)
    private val floats = FloatArray(32)
    private val evBuf = IntArray(64)
    private var controllerWasConnected = false
    private var lastOutputAttemptMs = 0L
    private var recUri: Uri? = null
    private var bannerUntil = 0L

    /** false enquanto o Raio-X está aberto (o Raio-X usa a saída do comando para os testes). */
    @Volatile var outputEnabled = true

    fun start() {
        settings.applyToNative()
        refreshPacks()
        startOutput()
    }

    fun refreshPacks() {
        packs = PackRepository.list(ctx)
    }

    fun showBanner(text: String, seconds: Int = 4, action: Pair<String, () -> Unit>? = null) {
        banner = text
        bannerAction = action
        bannerUntil = System.currentTimeMillis() + seconds * 1000L
    }

    // ------------------------------------------------------------ saída de som

    /** Saída principal = comando (jack + háptica). Se não der, altifalantes do telemóvel. */
    @Synchronized
    fun startOutput() {
        lastOutputAttemptMs = System.currentTimeMillis()
        stopOutput()
        val urbs = 4
        val packets = if (settings.usbQueueMs <= 4) 1 else 2
        val speaker = AudioDevices.speaker(ctx)?.id ?: 0
        if (settings.output == OutputChoice.CONTROLLER) {
            val claim = usb.claimOut()
            if (claim.isEmpty()) {
                val err = PerfBridge.startController(usb.fd, urbs, packets)
                if (err.isEmpty()) {
                    outputInfo = "Comando · jack + háptica · fila de ${urbs * packets} ms"
                    return
                }
                usb.releaseOut()
                Log.w(tag, "Saída do comando falhou: $err")
            }
            // Sem comando (ou falhou): a música continua nos altifalantes.
            val err = PerfBridge.startPhone(speaker, -1, urbs, packets, 0)
            outputInfo = if (err.isEmpty()) "Altifalantes do telemóvel (o comando não está disponível)" else "Sem saída de som: $err"
        } else {
            val claim = usb.claimOut()
            val fd = if (claim.isEmpty()) usb.fd else -1
            val err = PerfBridge.startPhone(speaker, fd, urbs, packets, settings.hapticDelayMs)
            if (err.isNotEmpty() && fd >= 0) usb.releaseOut()
            outputInfo = when {
                err.isNotEmpty() -> "Sem saída de som: $err"
                fd >= 0 -> "Altifalantes do telemóvel · háptica pelo comando"
                else -> "Altifalantes do telemóvel (sem háptica: comando não disponível)"
            }
        }
    }

    @Synchronized
    fun stopOutput() {
        PerfBridge.stopOutput()
        usb.releaseOut()
    }

    // ------------------------------------------------------------ packs

    fun selectPack(info: PackInfo) {
        if (loadingPack != null) return
        loadingPack = info.name
        val bpmForUser = currentPack?.bpm ?: 172f
        io.execute {
            val (h, warnings) = PackRepository.load(ctx, info, bpmForUser)
            // A troca acontece no próximo tempo 1 (a primeira vez entra já).
            var tries = 0
            while (!PerfBridge.packCommit(h, true) && tries++ < 200) Thread.sleep(50)
            if (tries >= 200) PerfBridge.packFree(h)
            currentPack = info
            loadingPack = null
            if (warnings.isNotEmpty()) {
                Log.w(tag, warnings.joinToString("\n"))
                showBanner("${info.name}: ${warnings.size} som(ns) não carregaram (ver docs/03-PACKS.md)", 6)
            }
        }
    }

    // ------------------------------------------------------------ ciclo (60×/s)

    fun poll() {
        PerfBridge.snapshot(ints, floats)
        PerfBridge.collect()
        snapshot = PerfSnapshot(
            hasPack = ints[0] == 1, barInPhrase = ints[1], beatInBar = ints[2], dropState = ints[3],
            rewinding = ints[4] != 0, muted = ints[5] == 1, restartPending = ints[6] == 1, packPending = ints[7] == 1,
            rollDiv = ints[8], fxType = ints[9], filterLocked = ints[10] == 1, shiftHeld = ints[11] == 1,
            padsHeld = ints[12], cuts = ints[13], micPad = ints[14], recording = ints[15] == 1, output = ints[16],
            loopState = ints.copyOfRange(17, 25), beatCount = ints[25], usbUnderruns = ints[26], usbErrors = ints[27],
            bpm = floats[0], beatPhase = floats[1], filter = floats[2], fxAmount = floats[3], volLoops = floats[4],
            volOne = floats[5], padLevel = floats.copyOfRange(7, 15), lastStrength = floats[15],
        )
        val n = PerfBridge.pollEvents(evBuf)
        for (i in 0 until n) handleEvent(evBuf[i * 2], evBuf[i * 2 + 1])
        if (banner != null && System.currentTimeMillis() > bannerUntil) {
            banner = null
            bannerAction = null
        }
        // Se a saída do comando caiu (cabo), a música passa para os altifalantes.
        if (outputEnabled && snapshot.output == PerfBridge.OUT_NONE && System.currentTimeMillis() - lastOutputAttemptMs > 300) {
            startOutput()
        }
    }

    private fun handleEvent(type: Int, value: Int) {
        when (type) {
            PerfBridge.EV_OPEN_PACKS -> openPacksRequest++
            PerfBridge.EV_REC_TOGGLE -> toggleSetRecording()
            PerfBridge.EV_MIC_START -> startMic(value)
            PerfBridge.EV_MIC_STOP -> stopMic(value)
            PerfBridge.EV_CONNECTED -> {
                if (value == 1) {
                    if (!controllerWasConnected) showBanner("Comando ligado", 2)
                    controllerWasConnected = true
                    if (settings.output == OutputChoice.CONTROLLER || PerfBridge.outputMode() != PerfBridge.OUT_CONTROLLER) {
                        // Religa: volta a pôr o som no comando (pequena espera para o USB estabilizar).
                        io.execute {
                            Thread.sleep(400)
                            startOutput()
                        }
                    }
                } else {
                    controllerWasConnected = false
                    showBanner("Comando desligado — a música continua nos altifalantes. Volta a ligar o cabo.", 6)
                }
            }
        }
    }

    // ------------------------------------------------------------ gravação do set

    fun toggleSetRecording() {
        if (snapshot.recording) {
            val secs = PerfBridge.recStop()
            recUri?.let { uri ->
                val v = ContentValues().apply { put(MediaStore.Audio.Media.IS_PENDING, 0) }
                ctx.contentResolver.update(uri, v, null, null)
            }
            recUri = null
            showBanner("Set gravado (%.0f s) em Música/OrimbuxPad".format(secs), 5)
            return
        }
        val name = "OrimbuxPad-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()) + ".wav"
        val values = ContentValues().apply {
            put(MediaStore.Audio.Media.DISPLAY_NAME, name)
            put(MediaStore.Audio.Media.MIME_TYPE, "audio/wav")
            put(MediaStore.Audio.Media.RELATIVE_PATH, Environment.DIRECTORY_MUSIC + "/OrimbuxPad")
            put(MediaStore.Audio.Media.IS_PENDING, 1)
        }
        val uri = ctx.contentResolver.insert(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, values)
        val pfd = uri?.let { ctx.contentResolver.openFileDescriptor(it, "rw") }
        if (uri == null || pfd == null) {
            showBanner("Não consegui criar o ficheiro da gravação", 4)
            return
        }
        if (PerfBridge.recStart(pfd.detachFd())) {
            recUri = uri
            showBanner("A gravar o set: $name", 3)
        } else {
            ctx.contentResolver.delete(uri, null, null)
            showBanner("A gravação não arrancou", 4)
        }
    }

    // ------------------------------------------------------------ micro do comando (Create + pad)

    @Volatile private var micRunning = false
    private var micThread: Thread? = null
    private var micData = ShortArray(0)
    private var micCount = 0

    @SuppressLint("MissingPermission")
    private fun startMic(pad: Int) {
        if (micRunning) return
        micRunning = true
        micData = ShortArray(48000 * 4)
        micCount = 0
        micThread = Thread({
            val rate = 48000
            val minBuf = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
            val rec = try {
                AudioRecord.Builder()
                    .setAudioSource(MediaRecorder.AudioSource.MIC)
                    .setAudioFormat(
                        AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(rate)
                            .setChannelMask(AudioFormat.CHANNEL_IN_MONO).build()
                    )
                    .setBufferSizeInBytes(max(minBuf, 4096) * 2)
                    .build()
            } catch (e: Exception) {
                Log.w(tag, "Micro: ${e.message}")
                micRunning = false
                return@Thread
            }
            AudioDevices.controllerInput(ctx)?.let { rec.setPreferredDevice(it) }
            rec.startRecording()
            val chunk = ShortArray(960)
            while (micRunning && micCount < micData.size) {
                val n = rec.read(chunk, 0, min(chunk.size, micData.size - micCount))
                if (n <= 0) break
                System.arraycopy(chunk, 0, micData, micCount, n)
                micCount += n
            }
            rec.stop()
            rec.release()
        }, "OrimbuxMic").also { it.start() }
        showBanner("A gravar pelo micro para o pad ${PackRepository.PAD_SYMBOLS[pad]}… (máx. 4 s)", 4)
    }

    private fun stopMic(pad: Int) {
        if (!micRunning && micThread == null) return
        micRunning = false
        io.execute {
            micThread?.join(1000)
            micThread = null
            val processed = processRecording(micData.copyOf(micCount))
            if (processed == null) {
                showBanner("Gravação vazia (não se ouviu nada)", 3)
                return@execute
            }
            val packId = currentPack?.id ?: PackRepository.DEMO_ID
            val dir = PackRepository.overrideDir(ctx, packId).apply { mkdirs() }
            val file = File(dir, "${PackRepository.PAD_KEYS[pad]}.wav")
            writeWav(file, processed)
            val err = PerfBridge.replaceOneshot(pad, file.absolutePath)
            if (err.isNotEmpty()) {
                showBanner("O som gravado não entrou: $err", 4)
                return@execute
            }
            showBanner(
                "Som gravado no pad ${PackRepository.PAD_SYMBOLS[pad]}", 8,
                "Guardar em \"Meus sons\"" to {
                    val mine = PackRepository.mySoundsDir(ctx).apply { mkdirs() }
                    file.copyTo(File(mine, file.name), overwrite = true)
                    refreshPacks()
                    showBanner("Guardado em \"Meus sons\"", 3)
                },
            )
        }
    }

    /** Corta o silêncio (−45 dBFS, com 5 ms de pré-roll), normaliza a −1 dBFS e faz fade-out de 5 ms. */
    private fun processRecording(s: ShortArray): ShortArray? {
        if (s.isEmpty()) return null
        val thr = (32768 * 0.0056).toInt()  // −45 dBFS
        var a = s.indexOfFirst { abs(it.toInt()) > thr }
        var b = s.indexOfLast { abs(it.toInt()) > thr }
        if (a < 0 || b <= a) return null
        a = max(0, a - 240)
        b = min(s.size - 1, b + 2400)
        val cut = s.copyOfRange(a, b + 1)
        val peak = cut.maxOf { abs(it.toInt()) }.coerceAtLeast(1)
        val g = 0.891 * 32767.0 / peak  // −1 dBFS
        val fade = min(240, cut.size)
        return ShortArray(cut.size) { i ->
            var v = cut[i] * g
            val fromEnd = cut.size - 1 - i
            if (fromEnd < fade) v *= fromEnd.toDouble() / fade
            v.coerceIn(-32768.0, 32767.0).toInt().toShort()
        }
    }

    private fun writeWav(file: File, mono: ShortArray) {
        val data = ByteBuffer.allocate(mono.size * 2).order(ByteOrder.LITTLE_ENDIAN)
        for (v in mono) data.putShort(v)
        val h = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN)
        h.put("RIFF".toByteArray()).putInt(36 + mono.size * 2).put("WAVE".toByteArray())
        h.put("fmt ".toByteArray()).putInt(16).putShort(1).putShort(1).putInt(48000).putInt(96000).putShort(2).putShort(16)
        h.put("data".toByteArray()).putInt(mono.size * 2)
        FileOutputStream(file).use {
            it.write(h.array())
            it.write(data.array())
        }
    }

    // ------------------------------------------------------------ sons gravados (Definições)

    fun overrides(): List<Pair<Int, File>> {
        val id = currentPack?.id ?: return emptyList()
        val dir = PackRepository.overrideDir(ctx, id)
        return (0 until 8).mapNotNull { p -> File(dir, "${PackRepository.PAD_KEYS[p]}.wav").takeIf { it.exists() }?.let { p to it } }
    }

    fun restoreOriginal(pad: Int) {
        val info = currentPack ?: return
        File(PackRepository.overrideDir(ctx, info.id), "${PackRepository.PAD_KEYS[pad]}.wav").delete()
        selectPack(info)  // recarrega o pack (a troca é no tempo 1)
    }

    fun shutdown() {
        if (snapshot.recording) toggleSetRecording()
        stopOutput()
        io.shutdown()
    }

    fun controllerConnected(): Boolean {
        val i = IntArray(16)
        NativeBridge.controllerState(i, FloatArray(24))
        return i[0] == 1 && i[13] == 1
    }
}
