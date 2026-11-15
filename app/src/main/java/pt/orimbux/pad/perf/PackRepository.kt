package pt.orimbux.pad.perf

import android.content.Context
import android.graphics.BitmapFactory
import android.graphics.Bitmap
import android.util.Log
import org.json.JSONObject
import java.io.File

/** Um som do pack (um dos 16 slots). */
data class SlotInfo(val pad: Int, val loop: Boolean, val path: String, val bars: Int, val gainDb: Float, val label: String)

enum class PackKind { DEMO, ASSET, USER }

data class PackInfo(
    val id: String,
    val name: String,
    val genre: String,
    val bpm: Float,
    val key: String,
    val color: Int,
    val coverAsset: String?,
    val slots: List<SlotInfo>,
    val kind: PackKind,
) {
    fun label(pad: Int, loop: Boolean): String =
        slots.firstOrNull { it.pad == pad && it.loop == loop }?.label ?: PackRepository.defaultLabel(pad, loop)
}

/**
 * Lista e carrega os packs:
 *  - "Pack de teste" (sintetizado pela app — funciona sem nada instalado);
 *  - packs em assets/packs/<id>/pack.json (não vão para o git; ver docs/03-PACKS.md);
 *  - "Meus sons" (one-shots gravados pelo micro do comando).
 * Sons gravados para um pack ficam em files/overrides/<id>/<pad>.wav e substituem o one-shot.
 */
object PackRepository {
    private const val TAG = "OrimbuxPacks"
    val PAD_KEYS = listOf("cross", "square", "circle", "triangle", "down", "left", "right", "up")
    val PAD_SYMBOLS = listOf("✕", "□", "○", "△", "↓", "←", "→", "↑")
    // Nomes curtos (cabem nos pads do ecrã de atuação).
    private val ONESHOT_NAMES = listOf("Kick", "Sub 808", "Snare", "Crash", "Acordes", "Synth", "Vocal", "Riser")
    private val LOOP_NAMES = listOf("Break", "Baixo", "Percussão", "Tops", "Pads", "Lead", "Vocal", "Atmosfera")

    fun defaultLabel(pad: Int, loop: Boolean) = if (loop) LOOP_NAMES[pad] else ONESHOT_NAMES[pad]

    const val DEMO_ID = "demo"
    const val MY_SOUNDS_ID = "meus-sons"

    fun overrideDir(ctx: Context, packId: String) = File(ctx.filesDir, "overrides/$packId")
    fun mySoundsDir(ctx: Context) = File(ctx.filesDir, "meus-sons")

    fun list(ctx: Context): List<PackInfo> {
        val out = mutableListOf<PackInfo>()
        out += PackInfo(DEMO_ID, "Pack de teste", "Sintetizado", 172f, "Fá menor", 0x00B4D8, null, emptyList(), PackKind.DEMO)
        val ids = try {
            ctx.assets.list("packs")?.toList().orEmpty()
        } catch (_: Exception) {
            emptyList()
        }
        for (id in ids.sorted()) {
            try {
                val json = ctx.assets.open("packs/$id/pack.json").bufferedReader().use { it.readText() }
                out += parse(id, JSONObject(json))
            } catch (e: Exception) {
                Log.w(TAG, "Pack $id ignorado: ${e.message}")
            }
        }
        val mine = mySoundsDir(ctx)
        val mineSlots = (0 until 8).mapNotNull { p ->
            val f = File(mine, "${PAD_KEYS[p]}.wav")
            if (f.exists()) SlotInfo(p, false, f.absolutePath, 0, 0f, "Gravação ${PAD_SYMBOLS[p]}") else null
        }
        if (mineSlots.isNotEmpty()) {
            out += PackInfo(MY_SOUNDS_ID, "Meus sons", "Gravações", 172f, "—", 0xE0218A, null, mineSlots, PackKind.USER)
        }
        return out
    }

    private fun parse(id: String, j: JSONObject): PackInfo {
        val slots = mutableListOf<SlotInfo>()
        val arr = j.getJSONArray("slots")
        for (i in 0 until arr.length()) {
            val s = arr.getJSONObject(i)
            val pad = PAD_KEYS.indexOf(s.getString("pad"))
            if (pad < 0) continue
            val loop = s.optString("type", "oneshot") == "loop"
            slots += SlotInfo(
                pad, loop, "asset:packs/$id/${s.getString("file")}", s.optInt("bars", 0),
                s.optDouble("gain_db", 0.0).toFloat(), s.optString("label", defaultLabel(pad, loop)),
            )
        }
        val color = try {
            Integer.parseInt(j.optString("color", "#00B4D8").removePrefix("#"), 16)
        } catch (_: Exception) {
            0x00B4D8
        }
        return PackInfo(
            j.optString("id", id), j.optString("name", id), j.optString("genre", ""), j.optDouble("bpm", 172.0).toFloat(),
            j.optString("key", ""), color, j.optString("cover", "").takeIf { it.isNotBlank() }?.let { "packs/$id/$it" },
            slots, PackKind.ASSET,
        )
    }

    fun cover(ctx: Context, info: PackInfo): Bitmap? = info.coverAsset?.let {
        try {
            ctx.assets.open(it).use { s -> BitmapFactory.decodeStream(s) }
        } catch (_: Exception) {
            null
        }
    }

    /**
     * Carrega o pack em memória nativa (chamar fora da thread principal).
     * Devolve o handle e a lista de avisos (sons que falharam).
     */
    fun load(ctx: Context, info: PackInfo, bpmForUserPack: Float): Pair<Long, List<String>> {
        val warnings = mutableListOf<String>()
        val bpm = if (info.kind == PackKind.USER) bpmForUserPack else info.bpm
        val h = PerfBridge.packCreate(info.id, bpm)
        if (info.kind == PackKind.DEMO) PerfBridge.packDemo(h)
        for (s in info.slots) {
            val err = PerfBridge.packSlot(h, s.pad, s.loop, s.path, s.bars, s.gainDb)
            if (err.isNotEmpty()) warnings += err
        }
        // Sons gravados pelo micro para este pack substituem o one-shot do pad.
        val od = overrideDir(ctx, info.id)
        for (p in 0 until 8) {
            val f = File(od, "${PAD_KEYS[p]}.wav")
            if (f.exists()) {
                val err = PerfBridge.packSlot(h, p, false, f.absolutePath, 0, 0f)
                if (err.isNotEmpty()) warnings += err
            }
        }
        return h to warnings
    }
}
