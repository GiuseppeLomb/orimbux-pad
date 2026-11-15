package pt.orimbux.pad.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.isActive
import pt.orimbux.pad.NativeBridge
import pt.orimbux.pad.perf.PerfBridge
import pt.orimbux.pad.perf.PerfController
import pt.orimbux.pad.raiox.AudioLab
import pt.orimbux.pad.raiox.DirectLab
import pt.orimbux.pad.raiox.DynamicsLab
import pt.orimbux.pad.raiox.RaioXModel
import pt.orimbux.pad.raiox.RaioXScreen
import pt.orimbux.pad.raiox.SmallButton

enum class Screen { PACKS, PERFORM, SETTINGS, RAIOX }

/** Estado do comando para os ecrãs (ligado, bateria, taxa de reports). */
data class PadInfo(val connected: Boolean = false, val battery: Int = -1, val reportHz: Float = 0f, val buttons: Int = 0)

// Bits dos botões (SDL_GamepadButton)
private const val BTN_CROSS = 0
private const val BTN_CIRCLE = 1
private const val BTN_OPTIONS = 6
private const val BTN_UP = 11
private const val BTN_DOWN = 12
private const val BTN_LEFT = 13
private const val BTN_RIGHT = 14

/** Navegação da app + ciclo de 60 Hz (estado do motor, eventos, navegação com o comando nos menus). */
@Composable
fun AppRoot(app: PerfController) {
    val ctx = LocalContext.current
    var screen by remember { mutableStateOf(Screen.PACKS) }
    var lastScreen by remember { mutableStateOf(Screen.PACKS) }
    var packSel by remember { mutableIntStateOf(0) }
    var pad by remember { mutableStateOf(PadInfo()) }

    // O Raio-X só se cria quando se abre (usa as mesmas peças do APK de diagnóstico).
    val raio = remember { mutableStateOf<RaioXParts?>(null) }

    // Modo do comando conforme o ecrã; ao entrar/sair do Raio-X a saída de som da app pára/retoma.
    LaunchedEffect(screen) {
        PerfBridge.setMode(
            when (screen) {
                Screen.PERFORM -> PerfBridge.MODE_PERFORM
                Screen.RAIOX -> PerfBridge.MODE_RAIOX
                else -> PerfBridge.MODE_MENU
            }
        )
        if (screen == Screen.RAIOX) {
            app.outputEnabled = false
            app.stopOutput()
            if (raio.value == null) raio.value = RaioXParts(ctx.applicationContext)
        } else if (lastScreen == Screen.RAIOX) {
            raio.value?.let {
                it.direct.stop()
                it.lab.closeAll()
            }
            NativeBridge.resetOutputs()
            app.outputEnabled = true
            app.startOutput()
            app.settings.applyToNative()
        }
        lastScreen = screen
    }

    LaunchedEffect(app.openPacksRequest) {
        if (app.openPacksRequest > 0 && screen == Screen.PERFORM) screen = Screen.PACKS
    }

    LaunchedEffect(Unit) {
        val ints = IntArray(16)
        val floats = FloatArray(24)
        var prev = 0
        while (isActive) {
            withFrameNanos { }
            app.poll()
            NativeBridge.controllerState(ints, floats)
            val buttons = ints[1]
            pad = PadInfo(ints[0] == 1 && ints[13] == 1, ints[2], floats[16], buttons)
            val edges = buttons and prev.inv()
            prev = buttons
            fun pressed(bit: Int) = (edges shr bit) and 1 == 1
            when (screen) {
                Screen.PACKS -> {
                    val n = app.packs.size
                    if (n > 0) {
                        if (pressed(BTN_RIGHT)) packSel = (packSel + 1) % n
                        if (pressed(BTN_LEFT)) packSel = (packSel - 1 + n) % n
                        if (pressed(BTN_DOWN)) packSel = minOf(n - 1, packSel + PACK_COLUMNS)
                        if (pressed(BTN_UP)) packSel = maxOf(0, packSel - PACK_COLUMNS)
                        if (pressed(BTN_CROSS)) {
                            app.selectPack(app.packs[packSel.coerceIn(0, n - 1)])
                            screen = Screen.PERFORM
                        }
                    }
                    if ((pressed(BTN_OPTIONS) || pressed(BTN_CIRCLE)) && app.snapshot.hasPack) screen = Screen.PERFORM
                }
                Screen.SETTINGS -> {
                    if (pressed(BTN_OPTIONS) || pressed(BTN_CIRCLE)) {
                        screen = if (app.snapshot.hasPack) Screen.PERFORM else Screen.PACKS
                    }
                }
                else -> {}
            }
        }
    }

    Box(Modifier.fillMaxSize().background(OrimbuxColors.Background)) {
        when (screen) {
            Screen.PACKS -> PacksScreen(
                app, packSel,
                onSelect = { i ->
                    packSel = i
                    app.selectPack(app.packs[i])
                    screen = Screen.PERFORM
                },
                onSettings = { screen = Screen.SETTINGS },
                onBack = { screen = Screen.PERFORM },
            )
            Screen.PERFORM -> PerformScreen(
                app, pad,
                onPacks = { screen = Screen.PACKS },
                onSettings = { screen = Screen.SETTINGS },
            )
            Screen.SETTINGS -> SettingsScreen(
                app,
                onBack = { screen = if (app.snapshot.hasPack) Screen.PERFORM else Screen.PACKS },
                onRaioX = { screen = Screen.RAIOX },
            )
            Screen.RAIOX -> {
                raio.value?.let { RaioXScreen(it.model, it.lab, it.direct, it.dyn) }
                Box(Modifier.align(Alignment.TopEnd).padding(8.dp)) {
                    SmallButton("Fechar Raio-X") { screen = Screen.SETTINGS }
                }
            }
        }
    }
}

/** Peças do Raio-X (criadas só quando se abre). */
class RaioXParts(ctx: android.content.Context) {
    val model = RaioXModel(ctx)
    val lab = AudioLab(ctx)
    val direct = DirectLab(ctx)
    val dyn = DynamicsLab()
}

const val PACK_COLUMNS = 4
