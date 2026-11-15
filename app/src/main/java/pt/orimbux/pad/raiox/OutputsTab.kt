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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.ui.OrimbuxColors

/** Separador "Saídas": rumble, luzes, LEDs e gatilhos — cada um com confirmação ✅/❌. */
@Composable
fun OutputsTab(model: RaioXModel) {
    var ledMask by remember { mutableIntStateOf(0) }
    var phraseRunning by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    Row(Modifier.fillMaxSize().padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Card("Rumble e luzes") {
                // Diagnóstico: o SDL aceitou os efeitos? (LEDs, mute LED, gatilhos, áudio do comando)
                val pad = model.pad
                val err = if (pad.effectsFailed > 0) " · último erro: " + NativeBridge.effectError() else ""
                Text(
                    "Efeitos enviados ao comando: ${pad.effectsSent} · recusados pelo SDL: ${pad.effectsFailed}$err",
                    fontSize = 12.sp,
                    color = if (pad.effectsFailed > 0) OrimbuxColors.Fail else OrimbuxColors.TextDim,
                )
                TestRow(model, "rumble_left") {
                    SmallButton("Testar 1 s") { NativeBridge.rumble(0xFFFF, 0, 1000) }
                }
                TestRow(model, "rumble_right") {
                    SmallButton("Testar 1 s") { NativeBridge.rumble(0, 0xFFFF, 1000) }
                }
                TestRow(model, "lightbar") {
                    SmallButton("R") { NativeBridge.lightbar(255, 0, 0) }
                    SmallButton("G") { NativeBridge.lightbar(0, 255, 0) }
                    SmallButton("B") { NativeBridge.lightbar(0, 0, 255) }
                    SmallButton("Off") { NativeBridge.lightbar(0, 0, 0) }
                }
                TestRow(model, "player_leds") {
                    // Cada botão acende SÓ esse LED (máscara escrita diretamente: bit 0 = esquerda).
                    for (i in 0 until 5) {
                        ChoiceChip("LED ${i + 1}", ledMask == (1 shl i)) {
                            ledMask = 1 shl i
                            NativeBridge.playerLeds(ledMask)
                        }
                    }
                    SmallButton("Todos") { ledMask = 0x1F; NativeBridge.playerLeds(0x1F) }
                    SmallButton("Nenhum") { ledMask = 0; NativeBridge.playerLeds(0) }
                }
                TestRow(model, "player_leds_phrase") {
                    SmallButton(if (phraseRunning) "A mostrar… (16 compassos, 2× mais rápido)" else "Mostrar a frase", enabled = !phraseRunning) {
                        phraseRunning = true
                        scope.launch {
                            // Mesmo padrão da app: exteriores (1,2,4,5) enchem 1 a cada 4 compassos;
                            // o central pisca em cada tempo (mais tempo no tempo 1). A 172 BPM, 2× mais rápido.
                            val beatMs = (60_000.0 / 172.0 / 2.0).toLong()
                            val outer = intArrayOf(0, 1, 3, 4)
                            for (bar in 0 until 16) {
                                var base = 0
                                for (k in 0 until (bar / 4 + 1)) base = base or (1 shl outer[k])
                                for (beat in 0 until 4) {
                                    val on = if (beat == 0) 100L else 40L
                                    NativeBridge.playerLeds(base or 0b00100)
                                    delay(on)
                                    NativeBridge.playerLeds(base)
                                    delay(beatMs - on)
                                }
                            }
                            NativeBridge.playerLeds(0)
                            phraseRunning = false
                        }
                    }
                }
                TestRow(model, "mute_led") {
                    SmallButton("Apagado") { NativeBridge.muteLed(0) }
                    SmallButton("Aceso") { NativeBridge.muteLed(1) }
                    SmallButton("Pulsar") { NativeBridge.muteLed(2) }
                }
                TestRow(model, "mute_led_kernel") {
                    SmallButton("Apagado") { NativeBridge.muteLedKernel(0) }
                    SmallButton("Aceso") { NativeBridge.muteLedKernel(1) }
                    SmallButton("Pulsar") { NativeBridge.muteLedKernel(2) }
                }
                Hint("Ao sair da app, o comando é reposto (luzes, gatilhos e rumble desligados).")
                SmallButton("Repor comando agora") { ledMask = 0; NativeBridge.resetOutputs() }
            }
        }
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Card("Gatilho L2 (carrega no L2 depois de ativar)") {
                TriggerRow(model, "l2_feedback", right = false, NativeBridge.TRIGGER_FEEDBACK)
                TriggerRow(model, "l2_weapon", right = false, NativeBridge.TRIGGER_WEAPON)
                TriggerRow(model, "l2_vibration", right = false, NativeBridge.TRIGGER_VIBRATION)
                TriggerRow(model, "l2_slope", right = false, NativeBridge.TRIGGER_SLOPE)
                TriggerRow(model, "l2_simple_res", right = false, NativeBridge.TRIGGER_SIMPLE_RESISTANCE)
                TriggerRow(model, "l2_simple_vib", right = false, NativeBridge.TRIGGER_SIMPLE_VIBRATION)
                TriggerRow(model, "l2_off", right = false, NativeBridge.TRIGGER_OFF)
            }
            Card("Gatilho R2 (carrega no R2 depois de ativar)") {
                TriggerRow(model, "r2_feedback", right = true, NativeBridge.TRIGGER_FEEDBACK)
                TriggerRow(model, "r2_weapon", right = true, NativeBridge.TRIGGER_WEAPON)
                TriggerRow(model, "r2_vibration", right = true, NativeBridge.TRIGGER_VIBRATION)
                TriggerRow(model, "r2_rhythmic", right = true, NativeBridge.TRIGGER_RHYTHMIC)
                TriggerRow(model, "r2_simple_res", right = true, NativeBridge.TRIGGER_SIMPLE_RESISTANCE)
                TriggerRow(model, "r2_simple_vib", right = true, NativeBridge.TRIGGER_SIMPLE_VIBRATION)
                TriggerRow(model, "r2_off", right = true, NativeBridge.TRIGGER_OFF)
            }
        }
    }
}

@Composable
private fun TriggerRow(model: RaioXModel, id: String, right: Boolean, mode: Int) {
    TestRow(model, id) {
        SmallButton("Ativar") { NativeBridge.trigger(right, mode) }
    }
}
