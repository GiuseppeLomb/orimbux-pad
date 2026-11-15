package pt.orimbux.pad.perf

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/** Saída de som principal. */
enum class OutputChoice { CONTROLLER, PHONE }

/**
 * Definições da app (guardadas em SharedPreferences) e aplicadas ao motor nativo.
 * Os valores por defeito seguem a especificação.
 */
class AppSettings(ctx: Context) {
    private val prefs = ctx.getSharedPreferences("orimbux", Context.MODE_PRIVATE)

    var output by mutableStateOf(OutputChoice.valueOf(prefs.getString("output", OutputChoice.CONTROLLER.name)!!))
    var usbQueueMs by mutableStateOf(prefs.getInt("usbQueueMs", 8))           // 8 ms (4 ms só se aprovado)
    var hapticDelayMs by mutableStateOf(prefs.getInt("hapticDelayMs", 10))    // modo altifalantes
    var leftHanded by mutableStateOf(prefs.getBoolean("leftHanded", false))
    var lightIntensity by mutableStateOf(prefs.getFloat("lightIntensity", 0.8f))
    var ledInvert by mutableStateOf(prefs.getBoolean("ledInvert", false))
    var hapticMax by mutableStateOf(prefs.getFloat("hapticMax", 0.4f))        // 40% por defeito
    var hapticProfile by mutableStateOf(prefs.getInt("hapticProfile", 1))     // 1 = transiente
    var metronomeOn by mutableStateOf(prefs.getBoolean("metronomeOn", true))
    var metronomeLevel by mutableStateOf(prefs.getFloat("metronomeLevel", 0.5f))
    var gyroSensitivity by mutableStateOf(prefs.getFloat("gyroSensitivity", 1.0f))
    var invertPitch by mutableStateOf(prefs.getBoolean("invertPitch", false))
    var invertRoll by mutableStateOf(prefs.getBoolean("invertRoll", false))
    var dynamicsOn by mutableStateOf(prefs.getBoolean("dynamicsOn", true))
    var dynLow by mutableStateOf(prefs.getFloat("dynLow", 0.15f))
    var dynHigh by mutableStateOf(prefs.getFloat("dynHigh", 3.0f))

    /** Gate (true) ou one-shot (false) por pad. Por defeito: gate nas setas, one-shot no losango. */
    val padGate = mutableStateListOf<Boolean>().apply {
        for (p in 0 until 8) add(prefs.getBoolean("gate$p", p >= 4))
    }

    /** Cores dos pads (0xRRGGBB), da especificação. */
    val padColors = intArrayOf(0x8B0000, 0x3A0CA3, 0xFF6B00, 0xFFE66D, 0x2D9D5C, 0x00B4D8, 0xE0218A, 0xE8F6FF)

    fun save() {
        prefs.edit().apply {
            putString("output", output.name)
            putInt("usbQueueMs", usbQueueMs)
            putInt("hapticDelayMs", hapticDelayMs)
            putBoolean("leftHanded", leftHanded)
            putFloat("lightIntensity", lightIntensity)
            putBoolean("ledInvert", ledInvert)
            putFloat("hapticMax", hapticMax)
            putInt("hapticProfile", hapticProfile)
            putBoolean("metronomeOn", metronomeOn)
            putFloat("metronomeLevel", metronomeLevel)
            putFloat("gyroSensitivity", gyroSensitivity)
            putBoolean("invertPitch", invertPitch)
            putBoolean("invertRoll", invertRoll)
            putBoolean("dynamicsOn", dynamicsOn)
            putFloat("dynLow", dynLow)
            putFloat("dynHigh", dynHigh)
            for (p in 0 until 8) putBoolean("gate$p", padGate[p])
        }.apply()
    }

    /** Envia tudo para o motor nativo. */
    fun applyToNative() {
        PerfBridge.settings(
            leftHanded, dynamicsOn, gyroSensitivity, invertPitch, invertRoll,
            lightIntensity, ledInvert, hapticMax, metronomeOn, metronomeLevel, hapticProfile,
        )
        for (p in 0 until 8) PerfBridge.setPadGate(p, padGate[p])
        PerfBridge.setPadColors(padColors)
        PerfBridge.setDynRange(dynLow, dynHigh)
    }

    /** Guarda os limites da dinâmica que o motor foi ajustando com o uso. */
    fun pullDynRange() {
        val v = FloatArray(2)
        PerfBridge.getDynRange(v)
        if (v[0] > 0f && v[1] > v[0]) {
            dynLow = v[0]
            dynHigh = v[1]
        }
    }

    fun changed() {
        save()
        applyToNative()
    }
}
