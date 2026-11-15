#!/usr/bin/env python3
"""
Orimbux Pad — criador de packs (corre no PC).

Passo 1 — analisar as pastas de samples e sugerir a atribuição dos 16 slots:

    python tools/packs/orimbux_pack.py scan --id dnb-essentials --name "DnB Essentials" \
        --genre "Drum & Bass" "C:/Samples/SampleRadar DnB" [--vocals "C:/Samples/Vocais"] [--bpm 172]

    → escreve packs-work/<id>/slots.txt. Abre-o, confirma/troca os ficheiros de cada slot e grava.

Passo 2 — gerar o pack a partir do ficheiro confirmado:

    python tools/packs/orimbux_pack.py build packs-work/<id>/slots.txt [--stretch]

    → converte para WAV 48 kHz 16 bits, normaliza, corta os loops em compassos exatos ao BPM
      (com 5 ms de cauda para o crossfade da app e micro-fades anti-clique), gera pack.json e
      copia tudo para app/src/main/assets/packs/<id>/ (pasta que NÃO vai para o git).

--stretch: se um loop vier noutro BPM, acerta-o no PC (offline). A app continua sem time-stretch.

Dependências: pip install -r tools/packs/requirements.txt
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

try:
    import numpy as np
    import soundfile as sf
    import librosa
except ImportError:  # pragma: no cover
    sys.exit("Faltam dependências. Corre:  pip install -r tools/packs/requirements.txt")

SR = 48000
TAIL = 240  # 5 ms de cauda para lá do último compasso (crossfade no ponto de loop)
ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "app" / "src" / "main" / "assets" / "packs"
WORK = ROOT / "packs-work"
AUDIO_EXT = {".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg"}

PADS = ["cross", "square", "circle", "triangle", "down", "left", "right", "up"]
PAD_SYMBOL = {"cross": "✕", "square": "□", "circle": "○", "triangle": "△", "down": "↓", "left": "←", "right": "→", "up": "↑"}
COLORS = {"Drum & Bass": "#8B0000", "Neurofunk": "#3A0CA3", "Jungle": "#2D9D5C", "Liquid": "#00B4D8"}

# Palavras-chave por slot (nome do ficheiro e das pastas). (pad, tipo) → lista
KEYWORDS = {
    ("cross", "oneshot"): ["kick", "kik", "bd", "bassdrum"],
    ("cross", "loop"): ["break", "drum loop", "drumloop", "drums", "beat", "amen", "full drum", "top+kick"],
    ("square", "oneshot"): ["sub", "808", "bass hit", "bass_hit", "bass one"],
    ("square", "loop"): ["bass", "reese", "neuro", "sub loop", "bassline"],
    ("circle", "oneshot"): ["snare", "sd", "clap"],
    ("circle", "loop"): ["perc", "percussion", "shaker", "conga", "rim", "bongo", "tamb"],
    ("triangle", "oneshot"): ["crash", "cymbal", "hat", "hihat", "hi-hat", "ride", "open"],
    ("triangle", "loop"): ["top", "tops", "hat loop", "hats", "ride loop", "hh loop"],
    ("down", "oneshot"): ["chord", "stab", "keys", "piano"],
    ("down", "loop"): ["pad", "pads", "chord", "keys", "piano", "strings", "rhodes"],
    ("left", "oneshot"): ["synth", "pluck", "lead", "stab", "zap"],
    ("left", "loop"): ["lead", "melody", "arp", "synth", "riff", "hook"],
    ("right", "oneshot"): ["vocal", "vox", "shout", "mc", "chop", "voice", "yeah", "hey"],
    ("right", "loop"): ["vocal", "vox", "voice", "chant", "mc"],
    ("up", "oneshot"): ["riser", "rise", "fx", "impact", "siren", "sweep", "uplifter", "horn", "airhorn", "downlifter"],
    ("up", "loop"): ["atmos", "atmosphere", "ambience", "ambient", "texture", "fx loop", "drone", "noise"],
}


# ----------------------------------------------------------------------------- utilitários

def load(path: Path) -> np.ndarray:
    """Áudio em float32, estéreo (2 × N), a 48 kHz."""
    y, sr = librosa.load(str(path), sr=SR, mono=False)
    if y.ndim == 1:
        y = np.vstack([y, y])
    return y[:2].astype(np.float32)


def bar_frames(bpm: float) -> float:
    return SR * 60.0 / bpm * 4.0


def bpm_from_name(name: str) -> float | None:
    m = re.search(r"(\d{2,3})(?:\.\d)?\s*-?\s*bpm", name, re.I) or re.search(r"[_\s-](1[5-8]\d)[_\s-]", " " + name + " ")
    if m:
        v = float(m.group(1))
        if 60 <= v <= 200:
            return v
    return None


def detect_bpm(y: np.ndarray) -> float | None:
    try:
        tempo, _ = librosa.beat.beat_track(y=y.mean(axis=0), sr=SR)
        t = float(np.atleast_1d(tempo)[0])
    except Exception:
        return None
    if t <= 0:
        return None
    while t < 140:  # DnB/Jungle: 160–175 (a análise às vezes dá metade)
        t *= 2
    while t > 200:
        t /= 2
    return t


def key_of(y: np.ndarray) -> tuple[str, float]:
    """Tom estimado pelo croma (Krumhansl). Devolve (tom, confiança 0..1). Baixa confiança = som "neutro"."""
    names = ["Dó", "Dó#", "Ré", "Ré#", "Mi", "Fá", "Fá#", "Sol", "Sol#", "Lá", "Lá#", "Si"]
    major = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
    minor = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
    try:
        chroma = librosa.feature.chroma_cqt(y=y.mean(axis=0), sr=SR).mean(axis=1)
    except Exception:
        return "—", 0.0
    best, best_r, scores = "—", -1.0, []
    for i in range(12):
        for prof, mode in ((major, "maior"), (minor, "menor")):
            r = float(np.corrcoef(np.roll(prof, i), chroma)[0, 1])
            scores.append(r)
            if r > best_r:
                best_r, best = r, f"{names[i]} {mode}"
    spread = best_r - float(np.median(scores))
    return best, max(0.0, min(1.0, spread * 2))


def centroid(y: np.ndarray) -> float:
    try:
        return float(np.median(librosa.feature.spectral_centroid(y=y.mean(axis=0), sr=SR)))
    except Exception:
        return 2000.0


def score(path: Path, pad: str, kind: str, cent: float) -> float:
    text = (path.parent.name + " " + path.stem).lower().replace("_", " ").replace("-", " ")
    s = 0.0
    for kw in KEYWORDS[(pad, kind)]:
        if kw in text:
            s += 3.0 if kw in path.stem.lower() else 1.5
    # Análise espectral: graves → kick/sub/baixo; agudos → hats/tops/crash
    if pad in ("cross", "square") and cent < 900:
        s += 1.0
    if pad == "triangle" and cent > 4000:
        s += 1.0
    return s


# ----------------------------------------------------------------------------- scan

def cmd_scan(args: argparse.Namespace) -> None:
    files = []
    for d in args.folders:
        files += [p for p in Path(d).rglob("*") if p.suffix.lower() in AUDIO_EXT]
    vocal_files = [p for p in Path(args.vocals).rglob("*") if p.suffix.lower() in AUDIO_EXT] if args.vocals else []
    if not files:
        sys.exit("Não encontrei ficheiros de áudio nessas pastas.")
    print(f"A analisar {len(files)} ficheiros (+{len(vocal_files)} vocais)…")

    infos = []
    for p in files + vocal_files:
        try:
            y = load(p)
        except Exception as e:
            print(f"  ignorado {p.name}: {e}")
            continue
        dur = y.shape[1] / SR
        bpm = bpm_from_name(p.name)
        infos.append({"path": p, "dur": dur, "bpm": bpm, "cent": centroid(y), "y": y, "vocal": p in vocal_files})

    # BPM do pack: indicado, ou a mediana dos loops (ficheiros com BPM no nome ou longos).
    pack_bpm = args.bpm
    if not pack_bpm:
        cands = [i["bpm"] for i in infos if i["bpm"]]
        if not cands:
            cands = [b for i in infos if i["dur"] > 2.0 and (b := detect_bpm(i["y"]))]
        pack_bpm = float(np.median(cands)) if cands else 172.0
    pack_bpm = round(pack_bpm, 1)
    bar_s = bar_frames(pack_bpm) / SR

    for i in infos:
        i["kind"] = "loop" if (i["dur"] >= 0.9 * bar_s or "loop" in i["path"].name.lower()) else "oneshot"
        if i["kind"] == "loop" and not i["bpm"]:
            i["bpm"] = detect_bpm(i["y"])

    # Vocais de outra fonte: tom/BPM (loops têm de ter o BPM do pack; one-shots podem ser neutros).
    keys = {}
    for i in infos:
        if i["vocal"]:
            keys[i["path"]] = key_of(i["y"])

    chosen = {}
    lines_cand = []
    for pad in PADS:
        for kind in ("oneshot", "loop"):
            pool = [i for i in infos if i["kind"] == kind and (pad == "right" or not i["vocal"])]
            if pad == "right":
                pool.sort(key=lambda i: (not i["vocal"]), reverse=False)
            ranked = sorted(pool, key=lambda i: score(i["path"], pad, kind, i["cent"]) + (2.0 if i["vocal"] and pad == "right" else 0), reverse=True)
            if kind == "loop":
                ranked = [i for i in ranked if not i["bpm"] or abs(i["bpm"] - pack_bpm) / pack_bpm < 0.02] or ranked
            if ranked:
                chosen[(pad, kind)] = ranked[0]
                alts = ", ".join(r["path"].name for r in ranked[1:4])
                if alts:
                    lines_cand.append(f"#   {pad}.{kind}: {alts}")

    out_dir = WORK / args.id
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / "slots.txt"
    color = COLORS.get(args.genre, "#00B4D8")
    with out.open("w", encoding="utf-8") as f:
        f.write(f"# Orimbux Pad — atribuição dos 16 slots do pack \"{args.name}\"\n")
        f.write("# Confirma ou troca o caminho de cada slot. Deixa vazio para um slot sem som.\n")
        f.write("# loops: 'bars = 0' = automático (o script mede o comprimento ao BPM do pack).\n")
        f.write(f"# Depois corre:  python tools/packs/orimbux_pack.py build \"{out}\"\n\n")
        f.write("[pack]\n")
        f.write(f"id = {args.id}\nname = {args.name}\ngenre = {args.genre}\nbpm = {pack_bpm}\nkey = {args.key or ''}\ncolor = {color}\ncover = {args.cover or ''}\n\n")
        f.write("[slots]\n")
        for pad in PADS:
            for kind in ("oneshot", "loop"):
                i = chosen.get((pad, kind))
                path = str(i["path"]).replace("\\", "/") if i else ""
                extra = ""
                if i and kind == "loop":
                    extra = f"   ; bars = 0 ; bpm do ficheiro = {i['bpm'] or '?'}"
                if i and i["vocal"]:
                    k, conf = keys.get(i["path"], ("—", 0))
                    extra += f"   ; vocal: tom {k if conf > 0.3 else 'neutro'}"
                f.write(f"{pad}.{kind} = {path}{extra}\n")
        f.write("\n# Alternativas encontradas (copia o caminho para cima se preferires):\n")
        f.write("\n".join(lines_cand) + "\n")
        if keys:
            f.write("\n# Vocais/MC analisados (tom estimado · 'neutro' = sem tom claro, serve em qualquer pack):\n")
            for p, (k, conf) in keys.items():
                f.write(f"#   {p.name}: {k if conf > 0.3 else 'neutro'}\n")
    print(f"BPM do pack: {pack_bpm}")
    print(f"Atribuição sugerida em: {out}\nConfirma o ficheiro e depois corre o comando 'build'.")


# ----------------------------------------------------------------------------- build

def parse_slots(path: Path) -> tuple[dict, dict]:
    pack, slots, section = {}, {}, None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        if line.strip().startswith("["):
            section = line.strip().strip("[]")
            continue
        if "=" not in line:
            continue
        key, val = line.split("=", 1)
        key = key.strip()
        parts = [p.strip() for p in val.split(";")]
        if section == "pack":
            pack[key] = parts[0]
        elif section == "slots":
            opts = {}
            for p in parts[1:]:
                if "=" in p:
                    k, v = p.split("=", 1)
                    opts[k.strip()] = v.strip()
            slots[key] = (parts[0], opts)
    return pack, slots


def fade(y: np.ndarray, n_in: int, n_out: int) -> np.ndarray:
    y = y.copy()
    if n_in > 0:
        y[:, :n_in] *= np.linspace(0, 1, n_in, dtype=np.float32)
    if n_out > 0:
        y[:, -n_out:] *= np.linspace(1, 0, n_out, dtype=np.float32)
    return y


def normalize(y: np.ndarray, peak_db: float = -1.0) -> np.ndarray:
    p = float(np.max(np.abs(y))) or 1.0
    return y * (10 ** (peak_db / 20) / p)


def process_loop(y: np.ndarray, pack_bpm: float, file_bpm: float | None, bars: int, stretch: bool) -> tuple[np.ndarray, int]:
    if stretch and file_bpm and abs(file_bpm - pack_bpm) / pack_bpm > 0.005:
        rate = pack_bpm / file_bpm
        print(f"    time-stretch offline {file_bpm} → {pack_bpm} BPM")
        try:
            import pyrubberband as pyrb  # melhor qualidade, se estiver instalado

            y = np.vstack([pyrb.time_stretch(y[c], SR, rate) for c in range(2)]).astype(np.float32)
        except Exception:
            y = np.vstack([librosa.effects.time_stretch(y[c], rate=rate) for c in range(2)]).astype(np.float32)
    elif file_bpm and abs(file_bpm - pack_bpm) / pack_bpm > 0.02:
        print(f"    AVISO: o loop está a {file_bpm} BPM e o pack a {pack_bpm} (usa --stretch ou outro loop)")
    bar = bar_frames(pack_bpm)
    if bars <= 0:
        raw = y.shape[1] / bar
        bars = min((1, 2, 4, 8, 16), key=lambda b: abs(b - raw))
    grid = int(round(bar * bars))
    need = grid + TAIL
    if y.shape[1] < need:  # curto demais: repete o início (o loop continua certo no compasso)
        reps = int(np.ceil(need / max(y.shape[1], 1)))
        y = np.tile(y, (1, reps))
    y = y[:, :need]
    y = fade(y, 48, 96)  # micro-fades anti-clique (1 ms / 2 ms) — a junção do loop é o crossfade da app
    return normalize(y), bars


def process_oneshot(y: np.ndarray) -> np.ndarray:
    mono = np.max(np.abs(y), axis=0)
    thr = 10 ** (-60 / 20)
    idx = np.where(mono > thr)[0]
    if idx.size:
        y = y[:, max(0, idx[0] - 48): idx[-1] + 1]
    y = y[:, : SR * 8]  # máximo 8 s
    return normalize(fade(y, 0, 240))


def cmd_build(args: argparse.Namespace) -> None:
    pack, slots = parse_slots(Path(args.slots))
    pid = pack.get("id") or sys.exit("Falta 'id' em [pack]")
    bpm = float(pack.get("bpm") or 172)
    if not 160 <= bpm <= 175:
        print(f"AVISO: o BPM {bpm} está fora de 160–175.")
    dest = ASSETS / pid
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    out_slots = []
    for key, (src, opts) in slots.items():
        if not src:
            continue
        pad, _, kind = key.partition(".")
        if pad not in PADS or kind not in ("oneshot", "loop"):
            print(f"  slot desconhecido: {key}")
            continue
        p = Path(src)
        if not p.exists():
            print(f"  FALTA o ficheiro de {key}: {p}")
            continue
        print(f"  {PAD_SYMBOL[pad]} {kind:7s} ← {p.name}")
        y = load(p)
        entry = {"pad": pad, "type": kind, "file": f"{pad}_{kind}.wav", "gain_db": float(opts.get("gain_db", 0)), "label": opts.get("label", p.stem[:24])}
        if kind == "loop":
            fb = bpm_from_name(p.name) or (detect_bpm(y) if not args.stretch else detect_bpm(y))
            y, bars = process_loop(y, bpm, fb, int(opts.get("bars", 0) or 0), args.stretch)
            entry["bars"] = bars
        else:
            y = process_oneshot(y)
        sf.write(str(dest / entry["file"]), y.T, SR, subtype="PCM_16")
        out_slots.append(entry)
    cover = pack.get("cover")
    if cover and Path(cover).exists():
        shutil.copy(cover, dest / ("cover" + Path(cover).suffix.lower()))
        cover = "cover" + Path(cover).suffix.lower()
    else:
        cover = ""
    meta = {
        "id": pid, "name": pack.get("name", pid), "genre": pack.get("genre", ""), "bpm": bpm,
        "key": pack.get("key", ""), "color": pack.get("color", "#00B4D8"), "cover": cover, "slots": out_slots,
    }
    (dest / "pack.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"\nPack pronto em {dest} ({len(out_slots)} sons).")
    print("Recompila a app para o incluir:  .\\tools\\build-apk.ps1")


def main() -> None:
    ap = argparse.ArgumentParser(description="Criador de packs do Orimbux Pad")
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("scan", help="analisar pastas e sugerir a atribuição dos slots")
    s.add_argument("folders", nargs="+")
    s.add_argument("--id", required=True)
    s.add_argument("--name", required=True)
    s.add_argument("--genre", required=True, help='"Drum & Bass", "Neurofunk", "Jungle" ou "Liquid"')
    s.add_argument("--bpm", type=float)
    s.add_argument("--key", default="")
    s.add_argument("--cover", default="")
    s.add_argument("--vocals", help="pasta extra de vocais/MC de qualquer fonte")
    b = sub.add_parser("build", help="gerar o pack a partir do slots.txt confirmado")
    b.add_argument("slots")
    b.add_argument("--stretch", action="store_true", help="acertar no PC o BPM de loops que venham noutro tempo")
    args = ap.parse_args()
    cmd_scan(args) if args.cmd == "scan" else cmd_build(args)


if __name__ == "__main__":
    main()
