package pt.orimbux.pad

import android.app.Activity
import android.util.Log
import org.libsdl.app.HIDDeviceManager
import org.libsdl.app.SDL

/**
 * Arranca a parte Java do SDL3 sem usar a SDLActivity.
 *
 * O SDL3 aceita um contexto que não seja a SDLActivity ("SDL or not", em SDL.setContext).
 * Ordem obrigatória:
 *  1. carregar libSDL3.so (o JNI_OnLoad regista os métodos nativos das classes Java do SDL);
 *  2. SDL.setupJNI() + SDL.initialize() + SDL.setContext(activity);
 *  3. HIDDeviceManager.acquire() — regista a ponte USB que o driver HIDAPI do SDL usa;
 *  4. só depois a thread nativa chama SDL_Init(SDL_INIT_GAMEPAD).
 */
object SdlBootstrap {
    private const val TAG = "OrimbuxSdl"
    private var librariesLoaded = false
    private var hidManager: HIDDeviceManager? = null

    fun init(activity: Activity) {
        if (!librariesLoaded) {
            System.loadLibrary("SDL3")
            System.loadLibrary("orimbux")
            SDL.setupJNI()
            librariesLoaded = true
        }
        SDL.initialize()
        SDL.setContext(activity)
        if (hidManager == null) {
            hidManager = HIDDeviceManager.acquire(activity)
        }
        Log.i(TAG, "SDL Java pronto")
    }

    fun shutdown() {
        hidManager?.let { HIDDeviceManager.release(it) }
        hidManager = null
        SDL.setContext(null)
    }
}
