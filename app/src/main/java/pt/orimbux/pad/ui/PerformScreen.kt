package pt.orimbux.pad.ui

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.perf.PackRepository
import pt.orimbux.pad.perf.PerfBridge
import pt.orimbux.pad.perf.PerfController
import pt.orimbux.pad.raiox.SmallButton
import kotlin.math.min

private val FX_NAMES = listOf("—", "Echo", "Reverb", "Flanger", "Bitcrush")
private val ROLL_NAMES = listOf("—", "1/4", "1/8", "1/16", "1/32")

/** Ecrã de atuação. */
@Composable
fun PerformScreen(app: PerfController, pad: PadInfo, onPacks: () -> Unit, onSettings: () -> Unit) {
    val s = app.snapshot
    val lh = app.settings.leftHanded
    val blink = rememberInfiniteTransition(label = "blink").animateFloat(
        initialValue = 0.25f, targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(220), RepeatMode.Reverse), label = "blinkA",
    ).value

    // Que pad está em cada posição física (destro: setas = pads 4–7, losango = 0–3; canhoto: espelho).
    val dpad = if (!lh) mapOf("up" to 7, "left" to 5, "right" to 6, "down" to 4) else mapOf("up" to 3, "left" to 2, "right" to 1, "down" to 0)
    val face = if (!lh) mapOf("up" to 3, "left" to 1, "right" to 2, "down" to 0) else mapOf("up" to 7, "left" to 6, "right" to 5, "down" to 4)

    Column(Modifier.fillMaxSize().padding(horizontal = 16.dp, vertical = 10.dp)) {
        TopBar(app, pad, onPacks, onSettings)
        Row(Modifier.weight(1f).fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Cluster(app, dpad, blink, Modifier.weight(1f).fillMaxHeight())
            CenterPanel(app, Modifier.weight(1.15f).fillMaxHeight())
            Cluster(app, face, blink, Modifier.weight(1f).fillMaxHeight())
        }
        Banner(app)
    }
}

@Composable
private fun TopBar(app: PerfController, pad: PadInfo, onPacks: () -> Unit, onSettings: () -> Unit) {
    val s = app.snapshot
    val p = app.currentPack
    Row(Modifier.fillMaxWidth().height(44.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(Modifier.weight(1f)) {
            Text(p?.name ?: "Sem pack", fontSize = 18.sp, fontWeight = FontWeight.Bold, maxLines = 1)
            Text(
                (p?.genre ?: "Escolhe um pack em Options") + " · ${s.bpm.toInt()} BPM" + (app.loadingPack?.let { " · a carregar $it…" } ?: "") +
                    (if (s.packPending) " · troca no tempo 1" else ""),
                fontSize = 12.sp, color = OrimbuxColors.TextDim, maxLines = 1,
            )
        }
        if (s.recording) {
            val a = rememberInfiniteTransition(label = "rec").animateFloat(0.3f, 1f, infiniteRepeatable(tween(500), RepeatMode.Reverse), label = "recA").value
            Box(Modifier.size(14.dp).alpha(a).clip(CircleShape).background(OrimbuxColors.Fail))
            Text("REC", color = OrimbuxColors.Fail, fontWeight = FontWeight.Bold, fontSize = 13.sp)
        }
        if (s.muted) Chip("MUTE", OrimbuxColors.Fail)
        Column(horizontalAlignment = Alignment.End) {
            Text(
                if (pad.connected) "Comando ligado" + (if (pad.battery >= 0) " · ${pad.battery}%" else "") else "Comando desligado",
                fontSize = 12.sp, color = if (pad.connected) OrimbuxColors.Ok else OrimbuxColors.Fail,
            )
            Text(
                app.outputInfo + if (pad.connected) " · %.0f Hz".format(pad.reportHz) else "",
                fontSize = 11.sp, color = if (pad.connected && pad.reportHz < 200f) OrimbuxColors.Fail else OrimbuxColors.TextDim, maxLines = 1,
            )
        }
        SmallButton("Packs") { onPacks() }
        SmallButton("Definições") { onSettings() }
    }
}

/** 4 pads em cruz/losango: posições cima, esquerda, direita, baixo. */
@Composable
private fun Cluster(app: PerfController, map: Map<String, Int>, blink: Float, modifier: Modifier) {
    BoxWithConstraints(modifier, contentAlignment = Alignment.Center) {
        val size: Dp = min(maxWidth.value / 3.1f, maxHeight.value / 3.1f).dp
        Box(Modifier.size(size * 3)) {
            PadTile(app, map.getValue("up"), blink, size, Modifier.offset(x = size, y = 0.dp))
            PadTile(app, map.getValue("left"), blink, size, Modifier.offset(x = 0.dp, y = size))
            PadTile(app, map.getValue("right"), blink, size, Modifier.offset(x = size * 2, y = size))
            PadTile(app, map.getValue("down"), blink, size, Modifier.offset(x = size, y = size * 2))
        }
    }
}

@Composable
private fun PadTile(app: PerfController, pad: Int, blink: Float, size: Dp, modifier: Modifier) {
    val s = app.snapshot
    val color = Color(0xFF000000.toInt() or app.settings.padColors[pad])
    val level = (s.padLevel[pad] * 6f).coerceIn(0f, 1f)
    val loop = s.loopState[pad]
    val held = (s.padsHeld shr pad) and 1 == 1
    val cut = (s.cuts shr pad) and 1 == 1
    val recording = s.micPad == pad
    val loopOn = loop == PerfBridge.LOOP_ON || loop == PerfBridge.LOOP_PENDING_OFF
    val pending = loop == PerfBridge.LOOP_PENDING_ON || loop == PerfBridge.LOOP_PENDING_OFF
    val borderColor = when {
        recording -> OrimbuxColors.Fail
        held -> Color.White
        loopOn || pending -> color
        else -> color.copy(alpha = 0.35f)
    }
    val borderAlpha = if (pending) blink else 1f
    val textColor = if (luminance(app.settings.padColors[pad]) > 0.6f && level > 0.5f) Color.Black else Color.White
    Box(
        modifier
            .size(size)
            .padding(5.dp)
            .clip(RoundedCornerShape(18.dp))
            .background(OrimbuxColors.Surface)
            .background(color.copy(alpha = (if (loopOn) 0.35f else 0.12f) + 0.6f * level))
            .border(if (loopOn || held || pending || recording) 3.dp else 1.dp, borderColor.copy(alpha = borderAlpha), RoundedCornerShape(18.dp))
            .alpha(if (cut) 0.35f else 1f),
    ) {
        Column(Modifier.fillMaxSize().padding(8.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
            Text(PackRepository.PAD_SYMBOLS[pad], fontSize = 26.sp, fontWeight = FontWeight.Bold, color = textColor)
            Text(app.currentPack?.label(pad, false) ?: PackRepository.defaultLabel(pad, false), fontSize = 12.sp, color = textColor, maxLines = 1, textAlign = TextAlign.Center)
            Text(
                (if (loopOn) "● " else "") + (app.currentPack?.label(pad, true) ?: PackRepository.defaultLabel(pad, true)),
                fontSize = 10.sp, color = textColor.copy(alpha = if (loopOn) 1f else 0.6f), maxLines = 1, textAlign = TextAlign.Center,
            )
        }
    }
}

@Composable
private fun CenterPanel(app: PerfController, modifier: Modifier) {
    val s = app.snapshot
    Column(modifier.padding(horizontal = 10.dp, vertical = 6.dp), verticalArrangement = Arrangement.spacedBy(10.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        // Frase de 16 compassos + tempos do compasso
        Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
            for (b in 0 until 16) {
                val on = s.hasPack && b <= s.barInPhrase
                Box(
                    Modifier.width(14.dp).height(18.dp).clip(RoundedCornerShape(3.dp))
                        .background(if (b == s.barInPhrase && s.hasPack) OrimbuxColors.Accent else if (on) OrimbuxColors.Accent.copy(alpha = 0.35f) else OrimbuxColors.SurfaceHigh)
                )
                if (b % 4 == 3 && b < 15) Spacer(Modifier.width(4.dp))
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            for (k in 0 until 4) {
                val on = s.hasPack && k == s.beatInBar
                Box(Modifier.size(if (k == 0) 16.dp else 12.dp).clip(CircleShape).background(if (on) Color.White else OrimbuxColors.SurfaceHigh))
            }
            Text("  Compasso ${s.barInPhrase + 1}/16", fontSize = 13.sp, color = OrimbuxColors.TextDim)
        }

        // Filtro bipolar
        Labeled("Filtro" + if (s.filterLocked) "  🔒 bloqueado" else "") { BipolarBar(s.filter, if (s.filter < 0) "low-pass" else if (s.filter > 0) "high-pass" else "sem filtro") }
        // Efeito do giroscópio
        Labeled("Efeito (R1 + inclinar)") {
            val active = s.fxAmount > 0.01f
            LevelBar(if (active) s.fxAmount else 0f, if (active) "${FX_NAMES[s.fxType]} ${(s.fxAmount * 100).toInt()}%" else "—", OrimbuxColors.Right)
        }
        // Roll / note repeat
        Labeled("Roll (R2)") {
            val notePad = if (s.rollDiv > 0) (0 until 8).lastOrNull { (s.padsHeld shr it) and 1 == 1 } else null
            Text(
                if (s.rollDiv == 0) "—" else "${ROLL_NAMES[s.rollDiv]}" + (notePad?.let { " · note repeat ${PackRepository.PAD_SYMBOLS[it]}" } ?: " · toda a música"),
                fontSize = 15.sp, fontWeight = FontWeight.Medium, color = if (s.rollDiv > 0) OrimbuxColors.Accent else OrimbuxColors.TextDim,
            )
        }
        // Volumes (touchpad)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Box(Modifier.weight(1f)) { Labeled("Vol. loops") { LevelBar(s.volLoops, "${(s.volLoops * 100).toInt()}%", OrimbuxColors.Down) } }
            Box(Modifier.weight(1f)) { Labeled("Vol. one-shots") { LevelBar(s.volOne, "${(s.volOne * 100).toInt()}%", OrimbuxColors.Circle) } }
        }
        // Estados
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            if (s.shiftHeld) Chip("LOOPS (L1)", OrimbuxColors.Accent)
            when (s.dropState) {
                1 -> Chip("DROP no tempo 1", OrimbuxColors.Triangle)
                2 -> Chip("DROP", OrimbuxColors.Triangle)
            }
            if (s.rewinding) Chip("REWIND", OrimbuxColors.Right)
            if (s.restartPending) Chip("RECOMEÇO no tempo 1", OrimbuxColors.Up)
            if (s.micPad >= 0) Chip("MICRO → ${PackRepository.PAD_SYMBOLS[s.micPad]}", OrimbuxColors.Fail)
        }
        if (app.settings.dynamicsOn && s.lastStrength > 0f) {
            Text("Força da última batida: %.2f m/s²".format(s.lastStrength), fontSize = 11.sp, color = OrimbuxColors.TextDim)
        }
        if (s.output == PerfBridge.OUT_CONTROLLER && (s.usbUnderruns > 0 || s.usbErrors > 0)) {
            Text("USB: ${s.usbUnderruns} cortes · ${s.usbErrors} erros", fontSize = 11.sp, color = OrimbuxColors.Fail)
        }
        if (!s.hasPack) {
            Text("Carrega em Options (ou em \"Packs\") para escolher um pack.", fontSize = 14.sp, color = OrimbuxColors.Accent, textAlign = TextAlign.Center)
        }
    }
}

@Composable
private fun Labeled(label: String, content: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(3.dp)) {
        Text(label, fontSize = 11.sp, color = OrimbuxColors.TextDim)
        content()
    }
}

@Composable
private fun LevelBar(v: Float, text: String, color: Color) {
    Box(Modifier.fillMaxWidth().height(22.dp).clip(RoundedCornerShape(6.dp)).background(OrimbuxColors.SurfaceHigh)) {
        Box(Modifier.fillMaxHeight().fillMaxWidth(v.coerceIn(0f, 1f)).background(color.copy(alpha = 0.7f)))
        Text(text, fontSize = 12.sp, modifier = Modifier.align(Alignment.Center))
    }
}

@Composable
private fun BipolarBar(v: Float, text: String) {
    BoxWithConstraints(Modifier.fillMaxWidth().height(22.dp).clip(RoundedCornerShape(6.dp)).background(OrimbuxColors.SurfaceHigh)) {
        val half = maxWidth / 2
        val w = half * kotlin.math.abs(v.coerceIn(-1f, 1f))
        Box(Modifier.offset(x = if (v < 0) half - w else half).width(w).fillMaxHeight().background(OrimbuxColors.Accent.copy(alpha = 0.7f)))
        Box(Modifier.offset(x = half - 1.dp).width(2.dp).fillMaxHeight().background(OrimbuxColors.TextDim))
        Text(text, fontSize = 12.sp, modifier = Modifier.align(Alignment.Center))
    }
}

@Composable
private fun Chip(text: String, color: Color) {
    Text(
        text, fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Color.Black,
        modifier = Modifier.clip(RoundedCornerShape(6.dp)).background(color).padding(horizontal = 7.dp, vertical = 3.dp),
    )
}

@Composable
private fun Banner(app: PerfController) {
    val b = app.banner ?: return
    Row(
        Modifier.fillMaxWidth().padding(top = 6.dp).clip(RoundedCornerShape(10.dp)).background(OrimbuxColors.SurfaceHigh).padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(b, fontSize = 13.sp, modifier = Modifier.weight(1f))
        app.bannerAction?.let { (label, action) -> SmallButton(label) { action() } }
    }
}

private fun luminance(rgb: Int): Float {
    val r = (rgb shr 16 and 0xFF) / 255f
    val g = (rgb shr 8 and 0xFF) / 255f
    val b = (rgb and 0xFF) / 255f
    return 0.2126f * r + 0.7152f * g + 0.0722f * b
}
