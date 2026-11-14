package pt.orimbux.pad

import android.content.Context

/** Versão da app (sem depender do BuildConfig gerado). */
object BuildConfigInfo {
    fun versionName(ctx: Context): String = try {
        ctx.packageManager.getPackageInfo(ctx.packageName, 0).versionName ?: "?"
    } catch (_: Exception) {
        "?"
    }
}
