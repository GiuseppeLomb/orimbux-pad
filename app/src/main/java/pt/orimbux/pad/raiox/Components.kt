package pt.orimbux.pad.raiox

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import pt.orimbux.pad.ui.OrimbuxColors

@Composable
fun StatusDot(status: Status, modifier: Modifier = Modifier) {
    val color = when (status) {
        Status.OK -> OrimbuxColors.Ok
        Status.FAIL -> OrimbuxColors.Fail
        Status.PENDING -> OrimbuxColors.Pending
    }
    Box(
        modifier
            .size(14.dp)
            .clip(CircleShape)
            .background(color)
    )
}

/** Cartão com título, usado para agrupar testes. */
@Composable
fun Card(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(OrimbuxColors.Surface)
            .padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(title, fontWeight = FontWeight.SemiBold, fontSize = 15.sp, color = OrimbuxColors.Text)
        content()
    }
}

/**
 * Linha de um teste: estado, nome, botões de ação e (se manual) a confirmação
 * "Funcionou / Não funcionou" que fica no relatório.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TestRow(
    model: RaioXModel,
    id: String,
    confirm: Boolean = true,
    actions: (@Composable RowScope.() -> Unit)? = null,
) {
    val def = RaioXTests.byId(id)
    val r = model.get(id)
    Column(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(OrimbuxColors.SurfaceHigh)
            .padding(horizontal = 12.dp, vertical = 6.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Row(
            Modifier.fillMaxWidth().heightIn(min = 34.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            StatusDot(r.status)
            Column(Modifier.weight(1f)) {
                Text(def.label, fontSize = 14.sp, color = OrimbuxColors.Text)
                if (r.detail.isNotBlank()) {
                    Text(r.detail, fontSize = 11.sp, color = OrimbuxColors.TextDim, maxLines = 4)
                }
            }
            if (confirm) {
                SmallButton("✅", tint = OrimbuxColors.Ok) { model.set(id, Status.OK, r.detail) }
                SmallButton("❌", tint = OrimbuxColors.Fail) { model.set(id, Status.FAIL, r.detail) }
            }
        }
        if (actions != null) {
            FlowRow(
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
                modifier = Modifier.padding(start = 22.dp),
            ) { actions() }
        }
    }
}

@Composable
fun SmallButton(text: String, tint: Color? = null, enabled: Boolean = true, onClick: () -> Unit) {
    if (tint != null) {
        OutlinedButton(
            onClick = onClick,
            enabled = enabled,
            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 2.dp),
            modifier = Modifier.heightIn(min = 34.dp),
        ) { Text(text, fontSize = 13.sp) }
    } else {
        FilledTonalButton(
            onClick = onClick,
            enabled = enabled,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 2.dp),
            modifier = Modifier.heightIn(min = 34.dp),
        ) { Text(text, fontSize = 13.sp) }
    }
}

@Composable
fun ChoiceChip(text: String, selected: Boolean, onClick: () -> Unit) {
    Button(
        onClick = onClick,
        colors = if (selected) ButtonDefaults.buttonColors() else ButtonDefaults.buttonColors(
            containerColor = OrimbuxColors.SurfaceHigh, contentColor = OrimbuxColors.Text
        ),
        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 2.dp),
        modifier = Modifier.heightIn(min = 34.dp),
    ) { Text(text, fontSize = 13.sp) }
}

@Composable
fun Hint(text: String) {
    Text(text, fontSize = 12.sp, color = OrimbuxColors.TextDim)
}

@Composable
fun Spacer8() = Box(Modifier.width(8.dp))

@Composable
fun MonoText(text: String) {
    Text(text, style = MaterialTheme.typography.bodySmall, color = OrimbuxColors.TextDim)
}
