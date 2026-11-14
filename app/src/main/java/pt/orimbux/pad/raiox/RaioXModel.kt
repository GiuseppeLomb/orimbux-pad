package pt.orimbux.pad.raiox

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import pt.orimbux.pad.NativeBridge
import java.io.File
import kotlin.math.abs
import kotlin.math.sqrt

/** SKIPPED = "não testado" de propósito (ex.: sem hub PD) — não conta como falha. */
enum class Status(val symbol: String) { PENDING("⏳"), OK("✅"), FAIL("❌"), SKIPPED("➖") }

data class TestResult(val status: Status = Status.PENDING, val detail: String = "")

/** Retrato do comando lido do motor nativo (60 vezes por segundo). */
data class PadSnapshot(
    val connected: Boolean = false,
    val buttons: Int = 0,
    val battery: Int = -1,
    val powerState: Int = 0,
    val touchDown: BooleanArray = BooleanArray(2),
    val addedCount: Int = 0,
    val removedCount: Int = 0,
    val sdlReady: Int = 0,
    val lastEffectOk: Boolean = true,
    val firmware: Int = 0,
    val vendor: Int = 0,
    val product: Int = 0,
    /** true = aberto pelo SDL por USB direto (HIDAPI); false = caminho do Android, sem luzes/gatilhos */
    val hidapi: Boolean = false,
    val axes: FloatArray = FloatArray(6),
    val gyro: FloatArray = FloatArray(3),
    val accel: FloatArray = FloatArray(3),
    val touchX: FloatArray = FloatArray(2),
    val touchY: FloatArray = FloatArray(2),
    val reportRateHz: Float = 0f,
) {
    fun pressed(bit: Int) = (buttons ushr bit) and 1 == 1

    companion object {
        // Bits = SDL_GamepadButton (ver controller.h)
        const val CROSS = 0; const val CIRCLE = 1; const val SQUARE = 2; const val TRIANGLE = 3
        const val CREATE = 4; const val PS = 5; const val OPTIONS = 6; const val L3 = 7; const val R3 = 8
        const val L1 = 9; const val R1 = 10; const val UP = 11; const val DOWN = 12; const val LEFT = 13
        const val RIGHT = 14; const val MUTE = 15; const val TOUCHPAD = 20; const val L2 = 28; const val R2 = 29
    }
}

/**
 * Estado do Raio-X: resultados de todos os testes (guardados em ficheiro para
 * sobreviverem a um reinício da app, por exemplo para testar o hub) e as
 * verificações automáticas das entradas.
 */
class RaioXModel(private val context: Context) {
    val results = mutableStateMapOf<String, TestResult>()
    var pad by mutableStateOf(PadSnapshot())
        private set

    private val ints = IntArray(16)
    private val floats = FloatArray(24)
    private val file = File(context.filesDir, "raio-x-resultados.txt")

    // Memória das verificações automáticas
    private val stickSeen = Array(2) { BooleanArray(4) } // [esq/dir][←, →, ↑, ↓]
    private val trigMid = BooleanArray(2)
    private val trigFull = BooleanArray(2)
    private var maxRate = 0f

    init {
        load()
        markHubScenariosNotTested()
    }

    /** Sem hub USB-C PD: os cenários com hub ficam "não testado" (não são falhas). */
    private fun markHubScenariosNotTested() {
        for (id in listOf("scn_a_hub", "scn_b_hub")) {
            if (get(id).status == Status.PENDING) set(id, Status.SKIPPED, "não testado: sem hub USB-C PD")
        }
    }

    fun get(id: String): TestResult = results[id] ?: TestResult()

    fun set(id: String, status: Status, detail: String = "") {
        val prev = results[id]
        if (prev?.status == status && prev.detail == detail) return
        results[id] = TestResult(status, detail)
        save()
    }

    /** Só marca ✅ uma vez (as verificações automáticas não "desmarcam"). */
    private fun autoOk(id: String, detail: String = "detetado") {
        if (get(id).status != Status.OK) set(id, Status.OK, detail)
    }

    fun resetAll() {
        results.clear()
        stickSeen.forEach { it.fill(false) }
        trigMid.fill(false); trigFull.fill(false); maxRate = 0f
        file.delete()
        markHubScenariosNotTested()
    }

    /** Lê o estado do comando e faz as verificações automáticas. */
    fun poll() {
        NativeBridge.controllerState(ints, floats)
        val s = PadSnapshot(
            connected = ints[0] == 1,
            buttons = ints[1],
            battery = ints[2],
            powerState = ints[3],
            touchDown = booleanArrayOf(ints[4] == 1, ints[5] == 1),
            addedCount = ints[6],
            removedCount = ints[7],
            sdlReady = ints[8],
            lastEffectOk = ints[9] == 1,
            firmware = ints[10],
            vendor = ints[11],
            product = ints[12],
            hidapi = ints[13] == 1,
            axes = floats.copyOfRange(0, 6),
            gyro = floats.copyOfRange(6, 9),
            accel = floats.copyOfRange(9, 12),
            touchX = floatArrayOf(floats[12], floats[14]),
            touchY = floatArrayOf(floats[13], floats[15]),
            reportRateHz = floats[16],
        )
        pad = s
        autoChecks(s)
    }

    private fun autoChecks(s: PadSnapshot) {
        if (s.sdlReady == -1 && get("sdl").status != Status.OK) {
            set("sdl", Status.FAIL, NativeBridge.controllerError())
        }
        // Só se testa com o comando aberto por HIDAPI (o caminho do Android é só transitório).
        if (!s.connected || !s.hidapi) return
        autoOk("sdl", "SDL3 HIDAPI · comando %04x:%04x · firmware 0x%04x".format(s.vendor, s.product, s.firmware))

        val map = mapOf(
            PadSnapshot.CROSS to "btn_cross", PadSnapshot.CIRCLE to "btn_circle",
            PadSnapshot.SQUARE to "btn_square", PadSnapshot.TRIANGLE to "btn_triangle",
            PadSnapshot.UP to "btn_up", PadSnapshot.DOWN to "btn_down",
            PadSnapshot.LEFT to "btn_left", PadSnapshot.RIGHT to "btn_right",
            PadSnapshot.L1 to "btn_l1", PadSnapshot.R1 to "btn_r1",
            PadSnapshot.L3 to "btn_l3", PadSnapshot.R3 to "btn_r3",
            PadSnapshot.PS to "btn_ps", PadSnapshot.CREATE to "btn_create",
            PadSnapshot.OPTIONS to "btn_options", PadSnapshot.MUTE to "btn_mute",
            PadSnapshot.TOUCHPAD to "btn_touchpad",
        )
        for ((bit, id) in map) if (s.pressed(bit)) autoOk(id)

        // Sticks: os 4 extremos de cada um
        for (side in 0..1) {
            val x = s.axes[side * 2]
            val y = s.axes[side * 2 + 1]
            if (x < -0.9f) stickSeen[side][0] = true
            if (x > 0.9f) stickSeen[side][1] = true
            if (y < -0.9f) stickSeen[side][2] = true
            if (y > 0.9f) stickSeen[side][3] = true
            if (stickSeen[side].all { it }) autoOk(if (side == 0) "stick_left" else "stick_right", "4 extremos")
        }
        // Gatilhos analógicos: meio curso e fundo
        for (side in 0..1) {
            val v = s.axes[4 + side]
            if (v in 0.3f..0.7f) trigMid[side] = true
            if (v > 0.95f) trigFull[side] = true
            if (trigMid[side] && trigFull[side]) autoOk(if (side == 0) "trig_l2" else "trig_r2", "0–100 %")
        }
        if (s.touchDown[0] || s.touchDown[1]) autoOk("touch_1")
        if (s.touchDown[0] && s.touchDown[1]) autoOk("touch_2")

        val g = sqrt(s.gyro[0] * s.gyro[0] + s.gyro[1] * s.gyro[1] + s.gyro[2] * s.gyro[2])
        if (g > 1.5f) autoOk("gyro", "rotação detetada")
        val a = sqrt(s.accel[0] * s.accel[0] + s.accel[1] * s.accel[1] + s.accel[2] * s.accel[2])
        if (abs(a - 9.81f) < 1.5f) autoOk("accel", "|a| = %.2f m/s²".format(a))
        if (s.battery >= 0) autoOk("battery", "${s.battery} %")

        if (s.reportRateHz > maxRate) maxRate = s.reportRateHz
        if (maxRate >= 200f && get("report_rate").detail != "%.0f Hz".format(maxRate)) {
            set("report_rate", Status.OK, "%.0f Hz".format(maxRate))
        }
        if (s.removedCount >= 1 && s.addedCount >= 2) autoOk("reconnect", "desligou e voltou a ligar sozinho")
    }

    // ---- persistência simples: id|estado|detalhe por linha ----
    private fun save() {
        try {
            file.writeText(results.entries.joinToString("\n") { (id, r) ->
                "$id|${r.status.name}|${r.detail.replace("\n", " ")}"
            })
        } catch (_: Exception) {
        }
    }

    private fun load() {
        if (!file.exists()) return
        file.readLines().forEach { line ->
            val parts = line.split("|", limit = 3)
            if (parts.size >= 2) {
                val st = runCatching { Status.valueOf(parts[1]) }.getOrNull() ?: return@forEach
                results[parts[0]] = TestResult(st, parts.getOrElse(2) { "" })
            }
        }
    }
}
