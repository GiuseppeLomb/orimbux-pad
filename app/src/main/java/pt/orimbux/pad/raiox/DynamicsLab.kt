package pt.orimbux.pad.raiox

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import pt.orimbux.pad.NativeBridge
import kotlin.math.abs
import kotlin.math.sqrt

/** Uma batida: força medida por 6 vias (acelerómetro/giroscópio × janela 0 / +4 / +12 ms). */
data class Hit(val seq: Int, val button: Int, val values: FloatArray)

/** As 6 medidas, pela ordem do array `values`. */
enum class HitMeasure(val label: String, val delayMs: Int, val unit: String) {
    A0("Acelerómetro · só o report do toque", 0, "m/s²"),
    A1("Acelerómetro · até +1 report", 4, "m/s²"),
    A3("Acelerómetro · até +3 reports", 12, "m/s²"),
    G0("Giroscópio · só o report do toque", 0, "rad/s"),
    G1("Giroscópio · até +1 report", 4, "rad/s"),
    G3("Giroscópio · até +3 reports", 12, "rad/s"),
}

data class MeasureAnalysis(
    val measure: HitMeasure,
    val weakMean: Float,
    val weakSd: Float,
    val strongMean: Float,
    val strongSd: Float,
    val separation: Float, // d de Cohen
    val accuracy: Float,   // melhor limiar: % de toques bem classificados
) {
    /** Fiável = 90% ou mais bem classificados e grupos bem separados. */
    val reliable get() = accuracy >= 0.9f && separation >= 1.5f
}

enum class DynPhase { IDLE, WEAK, STRONG }

/**
 * Teste de dinâmica: os botões do DualSense são digitais, mas uma batida forte abana
 * o comando. Mede-se esse abanão no acelerómetro/giroscópio e vê-se se distingue
 * 10 toques fracos de 10 fortes de forma fiável.
 */
class DynamicsLab {
    var phase by mutableStateOf(DynPhase.IDLE)
        private set
    val weak = mutableStateListOf<Hit>()
    val strong = mutableStateListOf<Hit>()
    var analysis by mutableStateOf<List<MeasureAnalysis>>(emptyList())
        private set

    private val buf = FloatArray(64 * 8)
    private var lastSeq = 0

    companion object {
        const val PER_GROUP = 10
    }

    private fun readHits(): List<Hit> {
        val n = NativeBridge.controllerHits(buf)
        return (0 until n).map { i ->
            val p = i * 8
            Hit(buf[p].toInt(), buf[p + 1].toInt(), buf.copyOfRange(p + 2, p + 8))
        }
    }

    /** Ignora batidas anteriores ao início de uma fase. */
    private fun syncSeq() {
        lastSeq = readHits().maxOfOrNull { it.seq } ?: lastSeq
    }

    fun startWeak() {
        weak.clear(); analysis = emptyList(); syncSeq(); phase = DynPhase.WEAK
    }

    fun startStrong() {
        strong.clear(); analysis = emptyList(); syncSeq(); phase = DynPhase.STRONG
    }

    fun reset() {
        weak.clear(); strong.clear(); analysis = emptyList(); phase = DynPhase.IDLE
    }

    fun poll(model: RaioXModel) {
        if (phase == DynPhase.IDLE) return
        for (h in readHits().filter { it.seq > lastSeq }.sortedBy { it.seq }) {
            lastSeq = h.seq
            val target = if (phase == DynPhase.WEAK) weak else strong
            if (target.size < PER_GROUP) target.add(h)
            if (target.size >= PER_GROUP) {
                phase = DynPhase.IDLE
                break
            }
        }
        if (phase == DynPhase.IDLE && weak.size >= PER_GROUP && strong.size >= PER_GROUP) analyze(model)
    }

    private fun analyze(model: RaioXModel) {
        val result = HitMeasure.entries.mapIndexed { idx, m ->
            val w = weak.map { it.values[idx] }
            val s = strong.map { it.values[idx] }
            val (mw, sw) = meanSd(w)
            val (ms, ss) = meanSd(s)
            val pooled = sqrt((sw * sw + ss * ss) / 2f).coerceAtLeast(1e-6f)
            MeasureAnalysis(m, mw, sw, ms, ss, (ms - mw) / pooled, bestAccuracy(w, s))
        }
        analysis = result
        // A melhor via fiável é a que tem menos atraso; em empate, a que acerta mais.
        val best = result.filter { it.reliable }.minWithOrNull(compareBy<MeasureAnalysis> { it.measure.delayMs }.thenByDescending { it.accuracy })
        if (best != null) {
            model.set(
                "dyn_force", Status.OK,
                "fiável: ${best.measure.label} (+${best.measure.delayMs} ms) · acerta %.0f%% · separação d=%.1f"
                    .format(best.accuracy * 100, best.separation),
            )
        } else {
            val top = result.maxByOrNull { it.accuracy }!!
            model.set(
                "dyn_force", Status.FAIL,
                "não fiável: a melhor via (${top.measure.label}) só acerta %.0f%% (d=%.1f) → volume fixo com ±3 dB"
                    .format(top.accuracy * 100, top.separation),
            )
        }
    }

    private fun meanSd(v: List<Float>): Pair<Float, Float> {
        if (v.isEmpty()) return 0f to 0f
        val m = v.average().toFloat()
        val sd = sqrt(v.map { (it - m) * (it - m) }.average().toFloat())
        return m to sd
    }

    /** Melhor limiar único "forte se valor ≥ limiar": fração de toques bem classificados. */
    private fun bestAccuracy(weak: List<Float>, strong: List<Float>): Float {
        val all = (weak + strong).sorted()
        var best = 0f
        val candidates = listOf(all.first() - 1f) + all.zipWithNext { a, b -> (a + b) / 2f } + listOf(all.last() + 1f)
        for (thr in candidates) {
            val ok = weak.count { it < thr } + strong.count { it >= thr }
            best = maxOf(best, ok.toFloat() / (weak.size + strong.size))
        }
        return best
    }

    fun format(v: Float) = if (abs(v) >= 10) "%.0f".format(v) else "%.2f".format(v)
}
