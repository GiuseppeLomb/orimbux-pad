package pt.orimbux.pad.ui

import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.Color

/** Cores da app: tema escuro; as cores dos pads seguem a especificação. */
object OrimbuxColors {
    val Background = Color(0xFF0B0B10)
    val Surface = Color(0xFF15151D)
    val SurfaceHigh = Color(0xFF20202B)
    val Text = Color(0xFFECECF2)
    val TextDim = Color(0xFF9A9AAE)
    val Ok = Color(0xFF2FBF71)
    val Fail = Color(0xFFE5484D)
    val Pending = Color(0xFF5C5C70)
    val Accent = Color(0xFF00B4D8)

    // Pads (camada base / loop)
    val Cross = Color(0xFF8B0000)
    val Square = Color(0xFF3A0CA3)
    val Circle = Color(0xFFFF6B00)
    val Triangle = Color(0xFFFFE66D)
    val Down = Color(0xFF2D9D5C)
    val Left = Color(0xFF00B4D8)
    val Right = Color(0xFFE0218A)
    val Up = Color(0xFFE8F6FF)
}

private val scheme = darkColorScheme(
    primary = OrimbuxColors.Accent,
    onPrimary = Color.Black,
    secondary = OrimbuxColors.Right,
    background = OrimbuxColors.Background,
    onBackground = OrimbuxColors.Text,
    surface = OrimbuxColors.Surface,
    onSurface = OrimbuxColors.Text,
    surfaceVariant = OrimbuxColors.SurfaceHigh,
    onSurfaceVariant = OrimbuxColors.TextDim,
    error = OrimbuxColors.Fail,
)

@Composable
fun OrimbuxTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = scheme) {
        // Cor por defeito dos textos (sem isto, os textos sem cor explícita saem a preto).
        CompositionLocalProvider(LocalContentColor provides OrimbuxColors.Text, content = content)
    }
}
