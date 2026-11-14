package pt.orimbux.pad.audio

import android.content.Context
import android.media.AudioDeviceInfo
import android.media.AudioManager

/**
 * Encontra as saídas/entradas de áudio relevantes:
 *  - altifalantes do telemóvel;
 *  - saída USB do DualSense (4 canais: jack/altifalante + háptica);
 *  - microfone USB do DualSense.
 */
object AudioDevices {

    private val controllerName = Regex("(?i)(wireless controller|dualsense|sony)")

    private fun isUsb(d: AudioDeviceInfo) =
        d.type == AudioDeviceInfo.TYPE_USB_HEADSET || d.type == AudioDeviceInfo.TYPE_USB_DEVICE

    private fun manager(ctx: Context) = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    fun speaker(ctx: Context): AudioDeviceInfo? =
        manager(ctx).getDevices(AudioManager.GET_DEVICES_OUTPUTS)
            .firstOrNull { it.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER }

    fun controllerOutput(ctx: Context): AudioDeviceInfo? {
        val usb = manager(ctx).getDevices(AudioManager.GET_DEVICES_OUTPUTS).filter(::isUsb)
        return usb.firstOrNull { controllerName.containsMatchIn(it.productName ?: "") } ?: usb.firstOrNull()
    }

    fun controllerInput(ctx: Context): AudioDeviceInfo? {
        val usb = manager(ctx).getDevices(AudioManager.GET_DEVICES_INPUTS).filter(::isUsb)
        return usb.firstOrNull { controllerName.containsMatchIn(it.productName ?: "") } ?: usb.firstOrNull()
    }

    fun typeName(type: Int): String = when (type) {
        AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "altifalante"
        AudioDeviceInfo.TYPE_USB_HEADSET -> "USB headset"
        AudioDeviceInfo.TYPE_USB_DEVICE -> "USB device"
        AudioDeviceInfo.TYPE_BUILTIN_MIC -> "micro interno"
        AudioDeviceInfo.TYPE_BLUETOOTH_A2DP -> "Bluetooth A2DP"
        AudioDeviceInfo.TYPE_BLE_HEADSET -> "Bluetooth LE"
        else -> "tipo $type"
    }

    /** Descrição técnica para o relatório (canais, máscaras de índice, frequências). */
    fun describe(d: AudioDeviceInfo?): String {
        if (d == null) return "não encontrado"
        val counts = d.channelCounts.sorted().joinToString(",").ifEmpty { "qualquer" }
        val idx = d.channelIndexMasks.joinToString(",") { "0x" + Integer.toHexString(it) }.ifEmpty { "—" }
        val rates = d.sampleRates.sorted().joinToString(",").ifEmpty { "qualquer" }
        return "\"${d.productName}\" (${typeName(d.type)}, id=${d.id}) · canais: $counts · máscaras de índice: $idx · Hz: $rates"
    }

    /** O dispositivo USB declara suporte a 4 canais? */
    fun supportsFourChannels(d: AudioDeviceInfo?): Boolean {
        if (d == null) return false
        if (d.channelCounts.isEmpty()) return true // "qualquer"
        return d.channelCounts.any { it >= 4 } || d.channelIndexMasks.any { Integer.bitCount(it) >= 4 }
    }
}
