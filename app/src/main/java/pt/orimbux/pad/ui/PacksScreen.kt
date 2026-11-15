package pt.orimbux.pad.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.perf.PackRepository
import pt.orimbux.pad.perf.PerfController
import pt.orimbux.pad.raiox.Hint
import pt.orimbux.pad.raiox.SmallButton

/** Ecrã de packs: grelha de capas (navegação com setas + ✕, ou por toque). */
@Composable
fun PacksScreen(app: PerfController, selected: Int, onSelect: (Int) -> Unit, onSettings: () -> Unit, onBack: () -> Unit) {
    val ctx = LocalContext.current
    val grid = rememberLazyGridState()
    LaunchedEffect(selected) { if (app.packs.isNotEmpty()) grid.animateScrollToItem(selected.coerceIn(0, app.packs.size - 1)) }
    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Packs", fontSize = 24.sp, fontWeight = FontWeight.Bold, modifier = Modifier.weight(1f))
            app.loadingPack?.let { Text("A carregar $it…", color = OrimbuxColors.Accent, fontSize = 14.sp) }
            if (app.snapshot.hasPack) SmallButton("Voltar à atuação") { onBack() }
            SmallButton("Definições") { onSettings() }
        }
        Hint("Setas + ✕ para escolher · ○ ou Options para voltar. A troca acontece no tempo 1 seguinte, sem cortar o som.")
        LazyVerticalGrid(
            columns = GridCells.Fixed(PACK_COLUMNS),
            state = grid,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            contentPadding = PaddingValues(bottom = 24.dp),
        ) {
            itemsIndexed(app.packs) { i, p ->
                val cover = remember(p.id) { PackRepository.cover(ctx, p) }
                val color = Color(0xFF000000.toInt() or p.color)
                val isCurrent = app.currentPack?.id == p.id
                Box(
                    Modifier
                        .fillMaxWidth()
                        .aspectRatio(1.35f)
                        .clip(RoundedCornerShape(16.dp))
                        .background(Brush.linearGradient(listOf(color.copy(alpha = 0.95f), color.copy(alpha = 0.35f), OrimbuxColors.Surface)))
                        .border(
                            if (i == selected) 3.dp else 1.dp,
                            if (i == selected) Color.White else color.copy(alpha = 0.5f),
                            RoundedCornerShape(16.dp),
                        )
                        .clickable { onSelect(i) },
                ) {
                    if (cover != null) {
                        Image(cover.asImageBitmap(), null, Modifier.fillMaxSize(), contentScale = ContentScale.Crop, alpha = 0.55f)
                    }
                    Column(Modifier.align(Alignment.BottomStart).padding(12.dp)) {
                        Text(p.name, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
                        Text("${p.genre} · ${p.bpm.toInt()} BPM${if (p.key.isNotBlank() && p.key != "—") " · ${p.key}" else ""}", fontSize = 13.sp, color = Color.White.copy(alpha = 0.85f))
                    }
                    if (isCurrent) {
                        Text(
                            "A TOCAR", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Color.Black,
                            modifier = Modifier.align(Alignment.TopEnd).padding(8.dp).clip(RoundedCornerShape(6.dp))
                                .background(OrimbuxColors.Ok).padding(horizontal = 6.dp, vertical = 2.dp),
                        )
                    }
                }
            }
        }
    }
}
