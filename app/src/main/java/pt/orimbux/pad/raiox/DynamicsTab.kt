package pt.orimbux.pad.raiox

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.ui.OrimbuxColors

/** Separador "Dinâmica": dá para ler a força da batida num pad pelos sensores? */
@Composable
fun DynamicsTab(model: RaioXModel, dyn: DynamicsLab) {
    Row(Modifier.fillMaxSize().padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Card("Força da batida nos pads") {
                Hint(
                    "Os botões do DualSense são digitais (sem pressão). Uma batida forte abana o comando: " +
                        "isto mede esse abanão no acelerómetro e no giroscópio, no report do próprio toque e nos seguintes."
                )
                Hint("Segura o comando como para tocar. Usa sempre o mesmo pad (por exemplo ✕), com o polegar.")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    SmallButton("1 · Gravar 10 toques FRACOS", enabled = dyn.phase == DynPhase.IDLE) { dyn.startWeak() }
                    SmallButton("2 · Gravar 10 toques FORTES", enabled = dyn.phase == DynPhase.IDLE) { dyn.startStrong() }
                    SmallButton("Recomeçar") { dyn.reset() }
                }
                val status = when (dyn.phase) {
                    DynPhase.WEAK -> "A gravar toques FRACOS: ${dyn.weak.size} / ${DynamicsLab.PER_GROUP}"
                    DynPhase.STRONG -> "A gravar toques FORTES: ${dyn.strong.size} / ${DynamicsLab.PER_GROUP}"
                    DynPhase.IDLE -> "Fracos: ${dyn.weak.size} / ${DynamicsLab.PER_GROUP} · fortes: ${dyn.strong.size} / ${DynamicsLab.PER_GROUP}"
                }
                Text(status, fontSize = 14.sp, fontWeight = FontWeight.Medium, color = OrimbuxColors.Accent)
                TestRow(model, "dyn_force", confirm = false)
            }
            Card("Valores de cada toque (acel. m/s² · giro. rad/s, em 0 / +4 / +12 ms)") {
                HitList("Fracos", dyn.weak, dyn)
                HitList("Fortes", dyn.strong, dyn)
            }
        }
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Card("Análise: dá para distinguir de forma fiável?") {
                if (dyn.analysis.isEmpty()) {
                    Hint("Aparece quando houver 10 toques fracos e 10 fortes.")
                } else {
                    for (a in dyn.analysis) {
                        Column {
                            Text(
                                "${if (a.reliable) "✅" else "❌"} ${a.measure.label} (+${a.measure.delayMs} ms)",
                                fontSize = 13.sp,
                                color = if (a.reliable) OrimbuxColors.Ok else OrimbuxColors.Text,
                            )
                            MonoText(
                                "fracos ${dyn.format(a.weakMean)} ± ${dyn.format(a.weakSd)} · fortes ${dyn.format(a.strongMean)} ± ${dyn.format(a.strongSd)} ${a.measure.unit}" +
                                    " · separação d=%.1f · acerta %.0f%%".format(a.separation, a.accuracy * 100)
                            )
                        }
                    }
                    Hint(
                        "Fiável = acerta em pelo menos 18 dos 20 toques e os grupos estão bem separados (d ≥ 1,5). " +
                            "A janela de 0 ms não atrasa o som; as de +4 e +12 ms só serviriam para ajustar o ganho depois do ataque."
                    )
                }
            }
        }
    }
}

@Composable
private fun HitList(title: String, hits: List<Hit>, dyn: DynamicsLab) {
    Text(title, fontSize = 13.sp, fontWeight = FontWeight.Medium)
    if (hits.isEmpty()) {
        Hint("—")
        return
    }
    hits.forEachIndexed { i, h ->
        val v = h.values
        Text(
            "%2d · a %s %s %s · g %s %s %s".format(
                i + 1, dyn.format(v[0]), dyn.format(v[1]), dyn.format(v[2]), dyn.format(v[3]), dyn.format(v[4]), dyn.format(v[5])
            ),
            fontFamily = FontFamily.Monospace, fontSize = 12.sp, color = OrimbuxColors.TextDim,
        )
    }
}
