package pt.orimbux.pad

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import pt.orimbux.pad.raiox.AudioLab
import pt.orimbux.pad.raiox.DirectLab
import pt.orimbux.pad.raiox.DynamicsLab
import pt.orimbux.pad.raiox.RaioXModel
import pt.orimbux.pad.raiox.RaioXScreen
import pt.orimbux.pad.ui.OrimbuxTheme

class MainActivity : ComponentActivity() {
    private lateinit var model: RaioXModel
    private lateinit var lab: AudioLab
    private lateinit var direct: DirectLab
    private lateinit var dyn: DynamicsLab
    private var engineStarted = false

    // O microfone é pedido antes de o SDL abrir o comando: o diálogo USB do Android
    // trata de forma diferente apps sem permissão de gravação num dispositivo com micro.
    private val micPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { startEngine() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Ecrã sempre ligado e em ecrã inteiro durante a atuação.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        // Carrega as bibliotecas nativas e a parte Java do SDL3.
        SdlBootstrap.init(this)
        model = RaioXModel(applicationContext)
        lab = AudioLab(applicationContext)
        direct = DirectLab(applicationContext)
        dyn = DynamicsLab()

        setContent { OrimbuxTheme { RaioXScreen(model, lab, direct, dyn) } }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startEngine()
        } else {
            micPermission.launch(Manifest.permission.RECORD_AUDIO)
        }
    }

    private fun startEngine() {
        if (engineStarted) return
        engineStarted = true
        NativeBridge.controllerStart()
        lab.refreshDevices()
    }

    override fun onDestroy() {
        if (engineStarted) {
            direct.stop() // devolve a interface de áudio ao Android
            lab.closeAll()
            NativeBridge.controllerStop() // repõe luzes, gatilhos e rumble
        }
        SdlBootstrap.shutdown()
        super.onDestroy()
    }

    // Enquanto o SDL ainda não reclamou o comando, o Android pode enviar teclas do gamepad
    // (ex.: ○ = "voltar"). Ignoram-se para não fecharem a app por acidente.
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (KeyEvent.isGamepadButton(event.keyCode) ||
            event.isFromSource(InputDevice.SOURCE_GAMEPAD) ||
            event.isFromSource(InputDevice.SOURCE_JOYSTICK)
        ) return true
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        if (ev.isFromSource(InputDevice.SOURCE_JOYSTICK)) return true
        return super.dispatchGenericMotionEvent(ev)
    }
}
