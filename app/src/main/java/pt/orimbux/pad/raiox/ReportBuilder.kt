package pt.orimbux.pad.raiox

import android.content.Context
import android.os.Build
import android.util.Log
import pt.orimbux.pad.BuildConfigInfo
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Gera o relatório ✅/❌ em texto (para copiar, partilhar ou ler com o adb). */
object ReportBuilder {

    fun build(ctx: Context, model: RaioXModel, lab: AudioLab): String {
        val sb = StringBuilder()
        val now = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date())
        val pad = model.pad
        sb.appendLine("ORIMBUX PAD — RELATÓRIO RAIO-X")
        sb.appendLine("Data: $now · App ${BuildConfigInfo.versionName(ctx)}")
        sb.appendLine("Telemóvel: ${Build.MANUFACTURER} ${Build.MODEL} · Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        sb.appendLine(
            "Comando: " + if (pad.vendor != 0) "%04x:%04x · firmware 0x%04x".format(pad.vendor, pad.product, pad.firmware)
            else "não ligado no momento do relatório"
        )
        sb.appendLine()
        sb.appendLine("Dispositivos de áudio:")
        sb.appendLine("  Altifalantes: ${lab.speakerInfo}")
        sb.appendLine("  Saída USB do comando: ${lab.ctrlOutInfo}")
        sb.appendLine("  Microfone do comando: ${lab.ctrlInInfo}")

        val all = RaioXTests.all
        var ok = 0
        var fail = 0
        var pending = 0
        var skipped = 0
        for (section in Section.entries) {
            sb.appendLine()
            sb.appendLine("== ${section.title.uppercase(Locale.ROOT)} ==")
            for (t in all.filter { it.section == section }) {
                val r = model.get(t.id)
                when (r.status) {
                    Status.OK -> ok++
                    Status.FAIL -> fail++
                    Status.PENDING -> pending++
                    Status.SKIPPED -> skipped++
                }
                sb.append(r.status.symbol).append(' ').append(t.label)
                if (r.detail.isNotBlank()) sb.append(" — ").append(r.detail)
                sb.appendLine()
            }
        }
        sb.appendLine()
        sb.appendLine("Resumo: $ok ✅ · $fail ❌ · $skipped ➖ não testados · $pending ⏳ por testar")
        return sb.toString()
    }

    /** Guarda em Android/data/pt.orimbux.pad/files/raio-x-relatorio.txt e no logcat. */
    fun save(ctx: Context, text: String): File? {
        Log.i("OrimbuxRaioX", text)
        return try {
            val dir = ctx.getExternalFilesDir(null) ?: ctx.filesDir
            File(dir, "raio-x-relatorio.txt").also { it.writeText(text) }
        } catch (_: Exception) {
            null
        }
    }
}
