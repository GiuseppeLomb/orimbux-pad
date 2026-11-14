package pt.orimbux.pad.raiox

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import pt.orimbux.pad.NativeBridge

/** Separador "Saídas": rumble, luzes, LEDs e gatilhos — cada um com confirmação ✅/❌. */
@Composable
fun OutputsTab(model: RaioXModel) {
    var ledMask by remember { mutableIntStateOf(0) }
    Row(Modifier.fillMaxSize().padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(
            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Card("Rumble e luzes") {
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
                    for (i in 0 until 5) {
                        val on = (ledMask shr i) and 1 == 1
                        ChoiceChip("${i + 1}", on) {
                            ledMask = ledMask xor (1 shl i)
                            NativeBridge.playerLeds(ledMask)
                        }
                    }
                }
                TestRow(model, "mute_led") {
                    SmallButton("Apagado") { NativeBridge.muteLed(0) }
                    SmallButton("Aceso") { NativeBridge.muteLed(1) }
                    SmallButton("Pulsar") { NativeBridge.muteLed(2) }
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
                TriggerRow(model, "l2_off", right = false, NativeBridge.TRIGGER_OFF)
            }
            Card("Gatilho R2 (carrega no R2 depois de ativar)") {
                TriggerRow(model, "r2_feedback", right = true, NativeBridge.TRIGGER_FEEDBACK)
                TriggerRow(model, "r2_weapon", right = true, NativeBridge.TRIGGER_WEAPON)
                TriggerRow(model, "r2_vibration", right = true, NativeBridge.TRIGGER_VIBRATION)
                TriggerRow(model, "r2_rhythmic", right = true, NativeBridge.TRIGGER_RHYTHMIC)
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
