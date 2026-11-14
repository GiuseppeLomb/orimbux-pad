package pt.orimbux.pad.audio

import android.annotation.SuppressLint
import android.media.AudioAttributes
import android.media.AudioDeviceInfo
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import kotlin.math.abs
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.sqrt

/**
 * Teste do microfone do comando: grava N segundos pela entrada USB do DualSense,
 * mede o nível e reproduz a gravação nos altifalantes do telemóvel.
 */
object MicTest {

    data class Result(
        val ok: Boolean,
        val peakDb: Double,
        val rmsDb: Double,
        val routedTo: String,
        val error: String? = null,
    )

    @SuppressLint("MissingPermission") // a permissão é pedida no arranque da app
    fun recordAndPlay(input: AudioDeviceInfo?, speaker: AudioDeviceInfo?, seconds: Int = 3): Result {
        if (input == null) return Result(false, -120.0, -120.0, "—", "Microfone do comando não encontrado")
        val rate = 48000
        val minBuf = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        val rec = try {
            AudioRecord.Builder()
                .setAudioSource(MediaRecorder.AudioSource.MIC)
                .setAudioFormat(
                    AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(rate)
                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                        .build()
                )
                .setBufferSizeInBytes(max(minBuf, 4096) * 2)
                .build()
        } catch (e: Exception) {
            return Result(false, -120.0, -120.0, "—", "Erro ao abrir o microfone: ${e.message}")
        }
        rec.setPreferredDevice(input)
        val data = ShortArray(rate * seconds)
        var read = 0
        rec.startRecording()
        val routed = rec.routedDevice
        while (read < data.size) {
            val n = rec.read(data, read, minOf(2048, data.size - read))
            if (n <= 0) break
            read += n
        }
        rec.stop()
        rec.release()

        var peak = 0
        var sumSq = 0.0
        for (i in 0 until read) {
            val v = abs(data[i].toInt())
            if (v > peak) peak = v
            sumSq += v.toDouble() * v
        }
        val peakDb = if (peak > 0) 20 * log10(peak / 32768.0) else -120.0
        val rms = if (read > 0) sqrt(sumSq / read) else 0.0
        val rmsDb = if (rms > 0) 20 * log10(rms / 32768.0) else -120.0
        val routedName = routed?.let { "${it.productName} (${AudioDevices.typeName(it.type)})" } ?: "?"

        // Reproduz o que foi gravado nos altifalantes do telemóvel.
        if (read > 0) {
            val track = AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build()
                )
                .setAudioFormat(
                    AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(rate).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build()
                )
                .setTransferMode(AudioTrack.MODE_STATIC)
                .setBufferSizeInBytes(read * 2)
                .build()
            speaker?.let { track.setPreferredDevice(it) }
            track.write(data, 0, read)
            track.play()
            Thread.sleep(seconds * 1000L + 200)
            track.stop()
            track.release()
        }
        // "Funciona" = veio pelo comando e captou som acima do ruído de fundo.
        val fromController = routed?.type == AudioDeviceInfo.TYPE_USB_HEADSET || routed?.type == AudioDeviceInfo.TYPE_USB_DEVICE
        return Result(fromController && peakDb > -50, peakDb, rmsDb, routedName)
    }
}
