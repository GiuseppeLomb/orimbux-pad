package pt.orimbux.pad.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.isActive
import pt.orimbux.pad.BuildConfigInfo
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.perf.OutputChoice
import pt.orimbux.pad.perf.PackRepository
import pt.orimbux.pad.perf.PerfBridge
import pt.orimbux.pad.perf.PerfController
import pt.orimbux.pad.raiox.Card
import pt.orimbux.pad.raiox.ChoiceChip
import pt.orimbux.pad.raiox.Hint
import pt.orimbux.pad.raiox.SmallButton
import java.io.File

/** Definições (tudo é guardado e aplicado logo). O Raio-X está no fim. */
@Composable
fun SettingsScreen(app: PerfController, onBack: () -> Unit, onRaioX: () -> Unit) {
    val st = app.settings
    val ctx = LocalContext.current
    fun changed() = st.changed()
    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Definições", fontSize = 24.sp, fontWeight = FontWeight.Bold, modifier = Modifier.weight(1f))
            Hint("○ ou Options para voltar   ")
            SmallButton("Voltar") { onBack() }
        }
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            // ---------------- coluna esquerda ----------------
            Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Card("Saída de som") {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        ChoiceChip("Comando: jack + háptica", st.output == OutputChoice.CONTROLLER) { st.output = OutputChoice.CONTROLLER; changed(); app.startOutput() }
                        ChoiceChip("Altifalantes do telemóvel", st.output == OutputChoice.PHONE) { st.output = OutputChoice.PHONE; changed(); app.startOutput() }
                    }
                    Hint(app.outputInfo)
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("Fila USB:", fontSize = 13.sp)
                        ChoiceChip("8 ms (recomendado)", st.usbQueueMs == 8) { st.usbQueueMs = 8; changed(); app.startOutput() }
                        ChoiceChip("4 ms (menos latência)", st.usbQueueMs == 4) { st.usbQueueMs = 4; changed(); app.startOutput() }
                    }
                    if (st.output == OutputChoice.PHONE) {
                        SliderRow("Calibração da latência: atraso da háptica", st.hapticDelayMs / 40f, "${st.hapticDelayMs} ms") {
                            st.hapticDelayMs = (it * 40).toInt(); changed()
                        }
                        SmallButton("Aplicar atraso") { app.startOutput() }
                    }
                }
                Card("Comando") {
                    SwitchRow("Modo canhoto (espelha o mapa todo)", st.leftHanded) { st.leftHanded = it; changed() }
                }
                Card("Luzes") {
                    SliderRow("Intensidade das luzes", st.lightIntensity, "${(st.lightIntensity * 100).toInt()}%") { st.lightIntensity = it; changed() }
                    SwitchRow("Inverter a ordem dos player LEDs", st.ledInvert) { st.ledInvert = it; changed() }
                    Hint("Usa se os LEDs da frase encherem da direita para a esquerda.")
                }
                Card("Vibração") {
                    SliderRow("Intensidade máxima da vibração", st.hapticMax, "${(st.hapticMax * 100).toInt()}%") { st.hapticMax = it; changed() }
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        ChoiceChip("Impulso transiente", st.hapticProfile == 1) { st.hapticProfile = 1; changed() }
                        ChoiceChip("Impulso suave (80 Hz)", st.hapticProfile == 0) { st.hapticProfile = 0; changed() }
                    }
                    SwitchRow("Metrónomo por vibração", st.metronomeOn) { st.metronomeOn = it; changed() }
                    if (st.metronomeOn) {
                        SliderRow("Intensidade do metrónomo", st.metronomeLevel, "${(st.metronomeLevel * 100).toInt()}%") { st.metronomeLevel = it; changed() }
                    }
                }
                Card("Giroscópio (R1 + inclinar · rewind)") {
                    SliderRow("Sensibilidade", (st.gyroSensitivity - 0.5f) / 1.5f, "×%.2f".format(st.gyroSensitivity)) {
                        st.gyroSensitivity = 0.5f + it * 1.5f; changed()
                    }
                    SwitchRow("Inverter frente/trás (echo ↔ reverb)", st.invertPitch) { st.invertPitch = it; changed() }
                    SwitchRow("Inverter esquerda/direita (flanger ↔ bitcrush)", st.invertRoll) { st.invertRoll = it; changed() }
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        SmallButton("Calibrar ponto neutro") {
                            PerfBridge.calibrateGyro()
                            app.showBanner("Ponto neutro calibrado (o comando tem de estar quieto)", 3)
                        }
                        Hint("Pousa o comando quieto e carrega.")
                    }
                }
            }
            // ---------------- coluna direita ----------------
            Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                DynamicsCard(app)
                Card("Modo de cada pad (one-shots)") {
                    Hint("Gate: toca enquanto seguras (fade-out de 15 ms ao largar). One-shot: toca até ao fim.")
                    for (p in 0 until 8) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text(
                                "${PackRepository.PAD_SYMBOLS[p]}  ${app.currentPack?.label(p, false) ?: PackRepository.defaultLabel(p, false)}",
                                fontSize = 13.sp, modifier = Modifier.weight(1f),
                            )
                            ChoiceChip("Gate", st.padGate[p]) { st.padGate[p] = true; changed() }
                            ChoiceChip("One-shot", !st.padGate[p]) { st.padGate[p] = false; changed() }
                        }
                    }
                }
                Card("Sons gravados (Create + pad)") {
                    val list = remember(app.currentPack, app.banner) { app.overrides() }
                    if (list.isEmpty()) Hint("Nenhum som gravado neste pack.")
                    for ((p, file) in list) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text("${PackRepository.PAD_SYMBOLS[p]}  gravação", fontSize = 13.sp, modifier = Modifier.weight(1f))
                            SmallButton("Guardar em Meus sons") {
                                val mine = PackRepository.mySoundsDir(ctx).apply { mkdirs() }
                                file.copyTo(File(mine, file.name), overwrite = true)
                                app.refreshPacks()
                                app.showBanner("Guardado em \"Meus sons\"", 3)
                            }
                            SmallButton("Repor original") { app.restoreOriginal(p) }
                        }
                    }
                }
                Card("Diagnóstico") {
                    Hint("O Raio-X testa todas as entradas e saídas do comando. A atuação pára enquanto está aberto.")
                    SmallButton("Abrir o Raio-X") { onRaioX() }
                    Hint("Versão ${BuildConfigInfo.versionName(ctx)}")
                }
            }
        }
    }
}

/** Calibração da dinâmica: 5 toques fracos + 5 fortes no mesmo pad → limites da curva. */
@Composable
private fun DynamicsCard(app: PerfController) {
    val st = app.settings
    var phase by remember { mutableIntStateOf(0) }  // 0 parado · 1 fracos · 2 fortes
    val weak = remember { mutableStateListOf<Float>() }
    val strong = remember { mutableStateListOf<Float>() }
    var lastSeq by remember { mutableIntStateOf(0) }
    val buf = remember { FloatArray(64 * 8) }
    fun readHits(): List<Pair<Int, Float>> {
        val n = NativeBridge.controllerHits(buf)
        return (0 until n).map { i -> buf[i * 8].toInt() to buf[i * 8 + 2] }  // (seq, força no report do toque)
    }
    LaunchedEffect(phase) {
        if (phase == 0) return@LaunchedEffect
        lastSeq = readHits().maxOfOrNull { it.first } ?: lastSeq
        while (isActive && phase != 0) {
            withFrameNanos { }
            for ((seq, a0) in readHits().filter { it.first > lastSeq }.sortedBy { it.first }) {
                lastSeq = seq
                val target = if (phase == 1) weak else strong
                if (target.size < 5) target.add(a0)
            }
            if (phase == 1 && weak.size >= 5) phase = 0
            if (phase == 2 && strong.size >= 5) {
                phase = 0
                val lo = weak.sorted()[2]
                val hi = strong.sorted()[2]
                if (hi > lo * 1.5f) {
                    st.dynLow = lo
                    st.dynHigh = hi
                    st.changed()
                    app.showBanner("Dinâmica calibrada: fraco %.2f · forte %.2f m/s²".format(lo, hi), 4)
                } else {
                    app.showBanner("Os toques fortes não se distinguiram dos fracos — tenta outra vez", 4)
                }
            }
        }
    }
    Card("Dinâmica dos pads (força da batida)") {
        SwitchRow("Volume e vibração seguem a força da batida", st.dynamicsOn) { st.dynamicsOn = it; st.changed() }
        if (!st.dynamicsOn) Hint("Desligada: volume fixo por pad com variação natural de ±3 dB.")
        LaunchedEffect(Unit) { st.pullDynRange() }
        Text("Limites atuais: fraco %.2f · forte %.2f m/s² (ajustam-se sozinhos com o uso)".format(st.dynLow, st.dynHigh), fontSize = 12.sp)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            SmallButton("1 · 5 toques fracos", enabled = phase == 0) { weak.clear(); phase = 1 }
            SmallButton("2 · 5 toques fortes", enabled = phase == 0 && weak.size >= 5) { strong.clear(); phase = 2 }
            SmallButton("Repor") { st.dynLow = 0.15f; st.dynHigh = 3.0f; st.changed() }
        }
        when (phase) {
            1 -> Text("Dá 5 toques FRACOS no mesmo pad: ${weak.size}/5", color = OrimbuxColors.Accent, fontSize = 13.sp)
            2 -> Text("Agora 5 toques FORTES: ${strong.size}/5", color = OrimbuxColors.Accent, fontSize = 13.sp)
        }
    }
}

@Composable
private fun SwitchRow(label: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, fontSize = 13.sp, modifier = Modifier.weight(1f))
        Switch(checked = value, onCheckedChange = onChange)
    }
}

@Composable
private fun SliderRow(label: String, value: Float, valueText: String, onChange: (Float) -> Unit) {
    Column(Modifier.fillMaxWidth()) {
        Row {
            Text(label, fontSize = 13.sp, modifier = Modifier.weight(1f))
            Text(valueText, fontSize = 13.sp, color = OrimbuxColors.Accent)
        }
        Slider(value = value.coerceIn(0f, 1f), onValueChange = onChange, modifier = Modifier.fillMaxWidth())
    }
}
