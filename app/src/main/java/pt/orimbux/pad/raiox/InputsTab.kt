package pt.orimbux.pad.raiox

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.ui.OrimbuxColors

/** Separador "Entradas": tudo o que o comando envia, em tempo real, + verificação automática. */
@Composable
fun InputsTab(model: RaioXModel) {
    val pad = model.pad
    Row(Modifier.fillMaxSize().padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        // ---- esquerda: visualização ao vivo ----
        Column(
            Modifier.weight(1.25f).fillMaxHeight().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Card("Botões (acendem enquanto carregas)") { ButtonsGrid(pad) }
            Card("Sticks, gatilhos e touchpad") {
                Row(horizontalArrangement = Arrangement.spacedBy(14.dp), verticalAlignment = Alignment.CenterVertically) {
                    StickView("Esq.", pad.axes[0], pad.axes[1])
                    TriggerBar("L2", pad.axes[4])
                    TouchpadView(pad, Modifier.weight(1f))
                    TriggerBar("R2", pad.axes[5])
                    StickView("Dir.", pad.axes[2], pad.axes[3])
                }
            }
            Card("Sensores e estado") {
                MonoText("Giroscópio (rad/s): x %+.2f  y %+.2f  z %+.2f".format(pad.gyro[0], pad.gyro[1], pad.gyro[2]))
                MonoText("Acelerómetro (m/s²): x %+.2f  y %+.2f  z %+.2f".format(pad.accel[0], pad.accel[1], pad.accel[2]))
                MonoText(
                    "Bateria: ${if (pad.battery >= 0) "${pad.battery} %" else "—"} (${powerText(pad.powerState)}) · " +
                        "reports: %.0f Hz · ligações: %d · desligações: %d".format(pad.reportRateHz, pad.addedCount, pad.removedCount)
                )
            }
        }
        // ---- direita: lista de verificações ----
        LazyColumn(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            item {
                Hint(
                    "Carrega em cada botão, leva os sticks aos 4 extremos, os gatilhos a meio e ao fundo, " +
                        "toca no touchpad com 1 e 2 dedos, roda o comando. Para a religação: tira o cabo e volta a pô-lo."
                )
            }
            items(RaioXTests.all.filter { it.section == Section.INPUTS }) { t ->
                TestRow(model, t.id, confirm = false)
            }
        }
    }
}

// Valores de SDL_PowerState
private fun powerText(state: Int) = when (state) {
    1 -> "a descarregar"
    2 -> "sem bateria"
    3 -> "a carregar"
    4 -> "carregada"
    else -> "?"
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ButtonsGrid(pad: PadSnapshot) {
    val buttons = listOf(
        "✕" to PadSnapshot.CROSS, "○" to PadSnapshot.CIRCLE, "□" to PadSnapshot.SQUARE, "△" to PadSnapshot.TRIANGLE,
        "↑" to PadSnapshot.UP, "↓" to PadSnapshot.DOWN, "←" to PadSnapshot.LEFT, "→" to PadSnapshot.RIGHT,
        "L1" to PadSnapshot.L1, "R1" to PadSnapshot.R1, "L2" to PadSnapshot.L2, "R2" to PadSnapshot.R2,
        "L3" to PadSnapshot.L3, "R3" to PadSnapshot.R3, "PS" to PadSnapshot.PS, "Create" to PadSnapshot.CREATE,
        "Options" to PadSnapshot.OPTIONS, "Mute" to PadSnapshot.MUTE, "Touchpad" to PadSnapshot.TOUCHPAD,
    )
    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        buttons.forEach { (label, bit) ->
            val on = pad.pressed(bit)
            Box(
                Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (on) OrimbuxColors.Accent else OrimbuxColors.SurfaceHigh)
                    .padding(horizontal = 10.dp, vertical = 6.dp)
            ) {
                Text(label, color = if (on) Color.Black else OrimbuxColors.Text, fontSize = 13.sp, fontWeight = FontWeight.Medium)
            }
        }
    }
}

@Composable
private fun StickView(label: String, x: Float, y: Float) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Canvas(Modifier.size(84.dp)) {
            val r = size.minDimension / 2f
            drawCircle(OrimbuxColors.SurfaceHigh, radius = r)
            drawCircle(OrimbuxColors.TextDim, radius = r * 0.3f, style = Stroke(1f)) // zona morta de referência
            drawCircle(OrimbuxColors.Accent, radius = 8.dp.toPx(), center = Offset(center.x + x * (r - 8.dp.toPx()), center.y + y * (r - 8.dp.toPx())))
        }
        Text("$label %+.2f %+.2f".format(x, y), fontSize = 11.sp, color = OrimbuxColors.TextDim)
    }
}

@Composable
private fun TriggerBar(label: String, v: Float) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.width(18.dp).height(84.dp).clip(RoundedCornerShape(6.dp)).background(OrimbuxColors.SurfaceHigh)) {
            Box(
                Modifier.align(Alignment.BottomCenter).fillMaxWidth().height((84f * v.coerceIn(0f, 1f)).dp)
                    .background(OrimbuxColors.Accent)
            )
        }
        Text("$label ${(v * 100).toInt()}%", fontSize = 11.sp, color = OrimbuxColors.TextDim)
    }
}

@Composable
private fun TouchpadView(pad: PadSnapshot, modifier: Modifier) {
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        Canvas(
            Modifier.fillMaxWidth().height(84.dp).clip(RoundedCornerShape(8.dp))
                .border(1.dp, OrimbuxColors.TextDim, RoundedCornerShape(8.dp))
        ) {
            drawRect(OrimbuxColors.SurfaceHigh, size = Size(size.width, size.height))
            val colors = listOf(OrimbuxColors.Right, OrimbuxColors.Triangle)
            for (f in 0..1) {
                if (pad.touchDown[f]) {
                    drawCircle(colors[f], radius = 9.dp.toPx(), center = Offset(pad.touchX[f] * size.width, pad.touchY[f] * size.height))
                }
            }
        }
        Text(
            "Touchpad: ${pad.touchDown.count { it }} dedo(s)",
            fontSize = 11.sp, color = OrimbuxColors.TextDim,
        )
    }
}
