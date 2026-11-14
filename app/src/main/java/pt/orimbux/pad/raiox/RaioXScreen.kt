package pt.orimbux.pad.raiox

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.isActive
import pt.orimbux.pad.ui.OrimbuxColors

/** Ecrã do Raio-X: 4 separadores (Entradas, Saídas, Áudio, Relatório). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RaioXScreen(model: RaioXModel, lab: AudioLab) {
    var tab by rememberSaveable { mutableIntStateOf(0) }

    // Lê o comando a cada frame (~60 Hz) e o estado do áudio ~5 vezes por segundo.
    LaunchedEffect(Unit) {
        var frame = 0
        while (isActive) {
            withFrameNanos { }
            model.poll()
            if (frame++ % 12 == 0) lab.pollNow()
        }
    }

    Column(Modifier.fillMaxSize().background(OrimbuxColors.Background)) {
        TopBar(model)
        PrimaryTabRow(selectedTabIndex = tab, containerColor = OrimbuxColors.Background) {
            listOf("Entradas", "Saídas", "Áudio e latência", "Relatório").forEachIndexed { i, title ->
                Tab(selected = tab == i, onClick = { tab = i }, text = { Text(title) })
            }
        }
        Box(Modifier.weight(1f)) {
            when (tab) {
                0 -> InputsTab(model)
                1 -> OutputsTab(model)
                2 -> AudioTab(model, lab)
                else -> ReportTab(model, lab)
            }
        }
    }
}

@Composable
private fun TopBar(model: RaioXModel) {
    val pad = model.pad
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Orimbux Pad · Raio-X", fontWeight = FontWeight.Bold, fontSize = 18.sp, modifier = Modifier.weight(1f))
        val (label, color) = when {
            pad.sdlReady == -1 -> "SDL falhou" to OrimbuxColors.Fail
            pad.connected -> "Comando ligado" + (if (pad.battery >= 0) " · ${pad.battery} %" else "") to OrimbuxColors.Ok
            else -> "Comando desligado — liga o cabo USB-C" to OrimbuxColors.Fail
        }
        StatusDot(if (color == OrimbuxColors.Ok) Status.OK else Status.FAIL)
        Text(label, fontSize = 14.sp, color = color)
    }
}

@Composable
private fun ReportTab(model: RaioXModel, lab: AudioLab) {
    val ctx = LocalContext.current
    var text by remember { mutableStateOf(ReportBuilder.build(ctx, model, lab)) }
    var confirmReset by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SmallButton("Atualizar") { text = ReportBuilder.build(ctx, model, lab) }
            SmallButton("Copiar") {
                text = ReportBuilder.build(ctx, model, lab)
                copy(ctx, text)
            }
            SmallButton("Partilhar") {
                text = ReportBuilder.build(ctx, model, lab)
                share(ctx, text)
            }
            SmallButton("Guardar ficheiro") {
                text = ReportBuilder.build(ctx, model, lab)
                val f = ReportBuilder.save(ctx, text)
                Toast.makeText(ctx, if (f != null) "Guardado: ${f.absolutePath}" else "Erro ao guardar", Toast.LENGTH_LONG).show()
            }
            if (!confirmReset) {
                SmallButton("Apagar resultados") { confirmReset = true }
            } else {
                SmallButton("Confirmar: apagar tudo", tint = OrimbuxColors.Fail) {
                    model.resetAll()
                    confirmReset = false
                    text = ReportBuilder.build(ctx, model, lab)
                }
            }
        }
        Box(
            Modifier.fillMaxSize().background(OrimbuxColors.Surface).padding(12.dp)
                .verticalScroll(rememberScrollState())
        ) {
            SelectionContainer {
                Text(text, fontFamily = FontFamily.Monospace, fontSize = 12.sp, color = OrimbuxColors.Text)
            }
        }
    }
}

private fun copy(ctx: Context, text: String) {
    val cm = ctx.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
    cm.setPrimaryClip(ClipData.newPlainText("Relatório Raio-X", text))
    ReportBuilder.save(ctx, text)
    Toast.makeText(ctx, "Relatório copiado", Toast.LENGTH_SHORT).show()
}

private fun share(ctx: Context, text: String) {
    ReportBuilder.save(ctx, text)
    val send = Intent(Intent.ACTION_SEND).apply {
        type = "text/plain"
        putExtra(Intent.EXTRA_SUBJECT, "Orimbux Pad — relatório Raio-X")
        putExtra(Intent.EXTRA_TEXT, text)
    }
    ctx.startActivity(Intent.createChooser(send, "Partilhar relatório").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
}
