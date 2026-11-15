package pt.orimbux.pad.audio

import android.content.Context
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.util.Log
import pt.orimbux.pad.NativeBridge

/**
 * Plano B — áudio USB direto para o DualSense (sem passar pelo sistema de áudio do Android).
 *
 * Abre uma segunda UsbDeviceConnection ao comando (o SDL3 tem a sua, só com a interface 3 HID),
 * reclama a interface de áudio com force=true (o kernel larga-a), escolhe a alternativa com o
 * endpoint isócrono e entrega o fd ao motor nativo (usb_direct.cpp).
 *
 *  - interface 1, alt 1: endpoint 0x01 OUT, 4 canais × 16 bits × 48 kHz (jack + háptica)
 *  - interface 2, alt 1: endpoint 0x82 IN,  2 canais × 16 bits × 48 kHz (microfone)
 */
class DirectUsbAudio(ctx: Context) {
    private val tag = "OrimbuxDirect"
    private val usb = ctx.getSystemService(Context.USB_SERVICE) as UsbManager
    private var conn: UsbDeviceConnection? = null
    private var outAlt0: UsbInterface? = null

    val isRunning get() = outAlt0 != null

    fun controller(): UsbDevice? = usb.deviceList.values.firstOrNull {
        it.vendorId == 0x054C && (it.productId == 0x0CE6 || it.productId == 0x0DF2)
    }

    private fun iface(dev: UsbDevice, id: Int, alt: Int): UsbInterface? =
        (0 until dev.interfaceCount).map { dev.getInterface(it) }.firstOrNull { it.id == id && it.alternateSetting == alt }

    private fun connection(dev: UsbDevice): UsbDeviceConnection? {
        conn?.let { return it }
        return usb.openDevice(dev)?.also { conn = it }
    }

    /** UAC1 SET_CUR da frequência de amostragem (48 kHz) no endpoint. Devolve o resultado do controlTransfer. */
    private fun setRate(c: UsbDeviceConnection, endpoint: Int): Int {
        val data = byteArrayOf(0x80.toByte(), 0xBB.toByte(), 0x00) // 48000 em 3 bytes little-endian
        return c.controlTransfer(0x22, 0x01, 0x0100, endpoint, data, data.size, 1000)
    }

    /** Começa a enviar áudio. Devolve um texto "OK …" ou "ERRO: …". */
    fun startOut(numUrbs: Int, packetsPerUrb: Int): String {
        if (isRunning) return "OK · já a correr"
        val dev = controller() ?: return "ERRO: comando não encontrado (está ligado?)"
        if (!usb.hasPermission(dev)) return "ERRO: a app não tem permissão USB para o comando"
        val c = connection(dev) ?: return "ERRO: openDevice falhou"
        val alt0 = iface(dev, 1, 0)
        val alt1 = iface(dev, 1, 1)
        if (alt0 == null || alt1 == null) return "ERRO: interface de áudio 1 não encontrada"
        if (!c.claimInterface(alt0, true)) {
            closeIfIdle()
            return "ERRO: claimInterface(1, force=true) recusado"
        }
        if (!c.setInterface(alt1)) {
            c.releaseInterface(alt0)
            closeIfIdle()
            return "ERRO: setInterface(interface 1, alternativa 1) recusado"
        }
        val rate = setRate(c, 0x01)
        val err = NativeBridge.directStart(c.fileDescriptor, 0x01, numUrbs, packetsPerUrb)
        if (err.isNotEmpty()) {
            c.setInterface(alt0)
            c.releaseInterface(alt0)
            closeIfIdle()
            return "ERRO: $err"
        }
        outAlt0 = alt0
        Log.i(tag, "Áudio direto a correr (SET_CUR 48 kHz = $rate)")
        return "OK · interface 1 reclamada sem root · fila de ${numUrbs * packetsPerUrb} ms · " +
            "SET_CUR 48 kHz: ${if (rate >= 0) "aceite" else "recusado ($rate)"}"
    }

    fun stopOut() {
        NativeBridge.directStop()
        val c = conn
        val alt0 = outAlt0
        if (c != null && alt0 != null) {
            c.setInterface(alt0)      // alternativa 0 = sem streaming
            c.releaseInterface(alt0)  // o Android volta a ligar o driver do kernel
        }
        outAlt0 = null
        closeIfIdle()
    }

    data class MicResult(val samples: ShortArray, val error: String)

    /** Grava o micro do comando diretamente pela interface 2 (estéreo intercalado). */
    fun recordMic(seconds: Int): MicResult {
        val dev = controller() ?: return MicResult(ShortArray(0), "comando não encontrado")
        if (!usb.hasPermission(dev)) return MicResult(ShortArray(0), "sem permissão USB")
        val c = connection(dev) ?: return MicResult(ShortArray(0), "openDevice falhou")
        val alt0 = iface(dev, 2, 0)
        val alt1 = iface(dev, 2, 1)
        if (alt0 == null || alt1 == null) return MicResult(ShortArray(0), "interface 2 não encontrada")
        if (!c.claimInterface(alt0, true)) {
            closeIfIdle()
            return MicResult(ShortArray(0), "claimInterface(2, force=true) recusado")
        }
        try {
            if (!c.setInterface(alt1)) return MicResult(ShortArray(0), "setInterface(interface 2, alternativa 1) recusado")
            setRate(c, 0x82)
            val data = NativeBridge.directRecord(c.fileDescriptor, 0x82, seconds)
            return MicResult(data, if (data.isEmpty()) NativeBridge.directRecordError() else "")
        } finally {
            c.setInterface(alt0)
            c.releaseInterface(alt0)
            closeIfIdle()
        }
    }

    private fun closeIfIdle() {
        if (outAlt0 == null) {
            conn?.close()
            conn = null
        }
    }
}
