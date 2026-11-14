package pt.orimbux.pad.audio

import android.media.AudioAttributes
import android.media.AudioDeviceInfo
import android.media.AudioFormat
import android.media.AudioTimestamp
import android.media.AudioTrack
import android.os.Process
import android.util.Log
import pt.orimbux.pad.NativeBridge

/**
 * Plano A, variante "AudioTrack": stream de 4 canais com máscara de índices 0xF
 * (o método que o Android documenta para placas USB multicanal).
 * O som é gerado pelo motor nativo (renderPull) e escrito aqui no AudioTrack.
 */
class PullQuadTrack {
    private val tag = "OrimbuxPull"
    @Volatile private var running = false
    private var thread: Thread? = null
    private var track: AudioTrack? = null

    fun start(device: AudioDeviceInfo, channels: Int = 4): String {
        stop()
        val format = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
            .setSampleRate(48000)
            .setChannelIndexMask((1 shl channels) - 1)
            .build()
        val t = try {
            AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build()
                )
                .setAudioFormat(format)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
                .build()
        } catch (e: Exception) {
            return "ERRO ao criar AudioTrack: ${e.message}"
        }
        t.setPreferredDevice(device)
        // Buffer pequeno para baixa latência (~8 ms), dentro do que o sistema aceitar.
        t.bufferSizeInFrames = 384
        val ch = t.channelCount
        NativeBridge.audioBeginPull(NativeBridge.PATH_CTRL, ch)
        track = t
        running = true
        t.play()
        thread = Thread({ loop(t, ch) }, "OrimbuxPullAudio").also { it.start() }
        // Dá tempo ao sistema para encaminhar o stream antes de ler o dispositivo final.
        Thread.sleep(150)
        val routed = t.routedDevice
        return "AudioTrack · máscara de índices 0x${Integer.toHexString((1 shl channels) - 1)}" +
            " · canais=$ch · encaminhado para: ${routed?.productName ?: "?"} (${routed?.let { AudioDevices.typeName(it.type) } ?: "?"})" +
            " · buffer=${t.bufferSizeInFrames} · baixa latência=${t.performanceMode == AudioTrack.PERFORMANCE_MODE_LOW_LATENCY}"
    }

    private fun loop(t: AudioTrack, ch: Int) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
        val frames = 96 // 2 ms por bloco
        val buf = FloatArray(frames * ch)
        val ts = AudioTimestamp()
        var blocks = 0
        try {
            while (running) {
                NativeBridge.audioRenderPull(NativeBridge.PATH_CTRL, buf, frames)
                t.write(buf, 0, buf.size, AudioTrack.WRITE_BLOCKING)
                if (++blocks % 10 == 0 && t.getTimestamp(ts)) {
                    NativeBridge.audioPullTimestamp(NativeBridge.PATH_CTRL, ts.framePosition, ts.nanoTime)
                }
            }
        } catch (e: Exception) {
            Log.w(tag, "Loop do AudioTrack terminou: ${e.message}")
        }
    }

    fun stop() {
        running = false
        thread?.join(500)
        thread = null
        track?.let {
            try {
                it.stop()
            } catch (_: Exception) {
            }
            it.release()
        }
        track = null
    }

    val isRunning get() = running
}
