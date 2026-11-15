package pt.orimbux.pad.raiox

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.ui.OrimbuxColors

/** Separador "Áudio": háptica, jack, altifalante, microfone e os cenários de latência A/B (com e sem hub). */
@Composable
fun AudioTab(model: RaioXModel, lab: AudioLab, direct: DirectLab) {
    Row(Modifier.fillMaxSize().padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Card("Dispositivos de áudio") {
                MonoText("Altifalantes: ${lab.speakerInfo}")
                MonoText("Saída do comando: ${lab.ctrlOutInfo}")
                MonoText("Microfone do comando: ${lab.ctrlInInfo}")
                if (!lab.ctrlOutFound) {
                    Text(
                        "O Android deste telemóvel não regista a saída de áudio do comando (só o microfone). " +
                            "Por este caminho, a háptica por áudio, o jack e o cenário B não podem funcionar. " +
                            "A alternativa é o plano B (áudio USB direto, sem passar pelo Android).",
                        fontSize = 12.sp,
                        color = OrimbuxColors.Fail,
                    )
                } else {
                    Text(
                        if (lab.ctrlSupportsFour) "O Android declara 4 canais para o comando ✔" else "O Android NÃO declara 4 canais para o comando",
                        fontSize = 12.sp,
                        color = if (lab.ctrlSupportsFour) OrimbuxColors.Ok else OrimbuxColors.Fail,
                    )
                }
                SmallButton("Atualizar") { lab.refreshDevices() }
            }
            Card("Método do stream de 4 canais para o comando") {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    CtrlMethod.entries.forEach { m ->
                        ChoiceChip(m.label, lab.method == m) { lab.method = m }
                    }
                }
                Hint("Se a háptica não se sentir com um método, experimenta os outros (reabre o cenário).")
            }
            Card("Testes rápidos do comando") {
                TestRow(model, "haptic_audio") {
                    SmallButton("Tocar háptica (0,4 s)") { lab.hapticTone() }
                }
                TestRow(model, "ctrl_jack") {
                    SmallButton("Tocar no jack (1 s)") { lab.jackTone() }
                }
                Hint("Para o jack: liga auscultadores ao comando.")
                TestRow(model, "ctrl_speaker") {
                    SmallButton("Tocar no altifalante (1 s)") { lab.speakerTone() }
                    SmallButton("Voltar ao jack") { lab.routeBackToJack() }
                }
                Hint("O altifalante do comando já não é usado pela app (o metrónomo passou a ser vibração). Fica só como diagnóstico.")
                TestRow(model, "ctrl_mic") {
                    SmallButton(if (lab.micBusy) "A gravar…" else "Gravar 3 s e ouvir", enabled = !lab.micBusy) {
                        lab.runMicTest { r ->
                            val detail = if (r.error != null) r.error else
                                "pico %.0f dBFS · RMS %.0f dBFS · entrada: %s".format(r.peakDb, r.rmsDb, r.routedTo)
                            model.set("ctrl_mic", if (r.ok) Status.OK else Status.FAIL, detail)
                        }
                    }
                }
                Hint("Fala para o comando durante a gravação; depois ouves a gravação no telemóvel.")
            }
        }
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            DirectProofCard(model, direct)
            ScenarioCard(model, lab)
        }
    }
}

/** Prova do plano B: áudio USB direto para o comando. */
@Composable
private fun DirectProofCard(model: RaioXModel, direct: DirectLab) {
    val v = direct.view
    Card("Prova do plano B — áudio USB direto (sem passar pelo Android)") {
        Hint(
            "A app reclama a interface de áudio 1 do comando e envia o som diretamente por USB. " +
                "Liga auscultadores ao jack do comando antes de começar."
        )
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            DirectBuffer.entries.forEach { b ->
                ChoiceChip("Fila ${b.label}", direct.buffer == b) { if (!v.running) direct.buffer = b }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            SmallButton("Iniciar", enabled = !v.running) { direct.start(model) }
            SmallButton("Parar", enabled = v.running) { direct.stop() }
            SmallButton("Limpar contadores") { direct.clear() }
        }
        if (direct.message.isNotBlank()) MonoText(direct.message)
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Switch(checked = direct.tone, onCheckedChange = { direct.setToneOn(it) })
            Text("Tom 440 Hz no jack (canais 1–2)", fontSize = 13.sp)
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Switch(checked = direct.pulse, onCheckedChange = { direct.setPulseOn(it) })
            Text("Pulso 80 Hz na háptica (canais 3–4), 120 ms por segundo", fontSize = 13.sp)
        }
        Text("Intensidade máxima da háptica: ${(direct.hapticLevel * 100).toInt()}%", fontSize = 13.sp)
        Slider(
            value = direct.hapticLevel,
            onValueChange = { direct.setHaptic(level = it) },
            valueRange = 0f..1f,
            steps = 9,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ChoiceChip("Impulso transiente (rápido)", direct.hapticProfile == 1) { direct.setHaptic(profile = 1) }
            ChoiceChip("Impulso 80 Hz suave (o de antes)", direct.hapticProfile == 0) { direct.setHaptic(profile = 0) }
        }
        Text(
            if (v.running) "A correr há %.0f s".format(v.elapsedS) else "Parado",
            fontSize = 13.sp, fontWeight = FontWeight.Medium,
            color = if (v.running) OrimbuxColors.Ok else OrimbuxColors.TextDim,
        )
        MonoText(v.healthText())
        Text("Latência botão→som no jack: ${v.latencyText()}", fontSize = 13.sp, color = OrimbuxColors.Accent)
        Hint("Carrega ✕ ○ □ △ ou as setas pelo menos 10 vezes: ouves um clique no jack e sentes um impulso nas mãos.")
        SmallButton(
            if (direct.stabilityRun) "Teste de 5 minutos a correr… %.0f / 300 s".format(v.elapsedS)
            else "Iniciar teste de 5 minutos (tom + pulso)",
            enabled = !direct.stabilityRun,
        ) { direct.startStability(model) }

        TestRow(model, "direct_claim", confirm = false)
        TestRow(model, "direct_jack")
        TestRow(model, "direct_haptic")
        TestRow(model, "direct_haptic_sync")
        Hint("Compara os dois impulsos a carregar nos pads: o transiente deve sentir-se ao mesmo tempo que o clique no jack.")
        TestRow(model, "direct_latency", confirm = false)
        TestRow(model, "direct_stability")
        Hint("No teste de 5 minutos, se ouvires algum corte ou estalido, marca ❌ mesmo que os contadores digam 0.")
        TestRow(model, "direct_sdl") {
            SmallButton("Lightbar R") { NativeBridge.lightbar(255, 0, 0) }
            SmallButton("G") { NativeBridge.lightbar(0, 255, 0) }
            SmallButton("B") { NativeBridge.lightbar(0, 0, 255) }
            SmallButton("R2 vibração") { NativeBridge.trigger(true, NativeBridge.TRIGGER_VIBRATION) }
            SmallButton("L2 rampa") { NativeBridge.trigger(false, NativeBridge.TRIGGER_SLOPE) }
            SmallButton("Gatilhos off") {
                NativeBridge.trigger(true, NativeBridge.TRIGGER_OFF)
                NativeBridge.trigger(false, NativeBridge.TRIGGER_OFF)
            }
        }
        TestRow(model, "direct_mic_android", confirm = false)
        TestRow(model, "direct_mic_usb", confirm = false) {
            SmallButton(if (direct.micBusy) "A gravar…" else "Gravar 3 s direto e ouvir", enabled = !direct.micBusy) {
                direct.recordMic(model)
            }
        }
        Hint("Ao parar, o Android recupera o áudio do comando. Se o micro não voltar a aparecer, tira e volta a pôr o cabo.")
    }
}

@Composable
private fun ScenarioCard(model: RaioXModel, lab: AudioLab) {
    var chosen by remember { mutableIntStateOf(NativeBridge.SCENARIO_A) }
    var heard by remember { mutableStateOf<Boolean?>(null) }
    var felt by remember { mutableStateOf<Boolean?>(null) }

    Card("Cenários de saída de som + latência botão→som") {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ChoiceChip("A · telemóvel + háptica", chosen == NativeBridge.SCENARIO_A) { chosen = NativeBridge.SCENARIO_A }
            ChoiceChip("B · jack + háptica (4 canais)", chosen == NativeBridge.SCENARIO_B) { chosen = NativeBridge.SCENARIO_B }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Switch(checked = lab.hub, onCheckedChange = { lab.hub = it })
            Text("Ligado por hub USB-C PD", fontSize = 13.sp)
            Text(
                if (lab.charging) "· telemóvel a carregar ✔" else "· telemóvel NÃO está a carregar",
                fontSize = 12.sp, color = if (lab.charging) OrimbuxColors.Ok else OrimbuxColors.TextDim,
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            SmallButton("Abrir cenário") { heard = null; felt = null; lab.openScenario(chosen) }
            SmallButton("Fechar") { lab.closeAll() }
            SmallButton("Limpar medições") { lab.clearLatency() }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Switch(checked = lab.testMusic, onCheckedChange = { lab.setMusic(it) })
            Text("Música de teste (kick + hats a 172 BPM; o kick vai para a háptica)", fontSize = 13.sp)
        }
        if (lab.openMessage.isNotBlank()) MonoText(lab.openMessage)
        MonoText("Stream telemóvel: ${lab.phoneStatus}")
        MonoText("Stream comando: ${lab.ctrlStatus}")

        Text(
            "Carrega ✕ ○ □ △ ou as setas pelo menos 10 vezes, espaçadas.",
            fontSize = 13.sp, fontWeight = FontWeight.Medium,
        )
        if (lab.scenario == NativeBridge.SCENARIO_A) {
            Text("Telemóvel: ${lab.phoneLatency.text()}", fontSize = 13.sp, color = OrimbuxColors.Accent)
        }
        Text("Comando: ${lab.ctrlLatency.text()}", fontSize = 13.sp, color = OrimbuxColors.Accent)
        Hint(
            "Latência = do momento em que o SDL recebe o botão até o som sair no DAC (timestamp do sistema). " +
                "Some-lhe 0–4 ms do USB do comando (média ~2 ms), que o telemóvel não consegue medir."
        )

        val soundLabel = if (chosen == NativeBridge.SCENARIO_A) "Ouvi o clique no telemóvel" else "Ouvi o clique no jack"
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(soundLabel, fontSize = 13.sp, modifier = Modifier.weight(1f))
            ChoiceChip("✅", heard == true) { heard = true }
            ChoiceChip("❌", heard == false) { heard = false }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Senti a háptica no comando", fontSize = 13.sp, modifier = Modifier.weight(1f))
            ChoiceChip("✅", felt == true) { felt = true }
            ChoiceChip("❌", felt == false) { felt = false }
        }
        val target = when {
            chosen == NativeBridge.SCENARIO_A && !lab.hub -> "scn_a"
            chosen == NativeBridge.SCENARIO_A -> "scn_a_hub"
            !lab.hub -> "scn_b"
            else -> "scn_b_hub"
        }
        SmallButton("Guardar resultado em: ${RaioXTests.byId(target).label}", enabled = heard != null && felt != null) {
            val ok = heard == true && felt == true
            val detail = "${lab.scenarioSummary()} · som: ${if (heard == true) "sim" else "não"} · háptica: ${if (felt == true) "sim" else "não"}"
            model.set(target, if (ok) Status.OK else Status.FAIL, detail)
        }
        TestRow(model, target, confirm = false)
    }
}
