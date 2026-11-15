# Packs — como adicionar

Os packs **não vão para o git**, porque as licenças são "royalty-free, sem redistribuição". Ficam só na tua build local, em `app/src/main/assets/packs/` (pasta ignorada pelo git).

A app traz sempre o **Pack de teste** (sintetizado por código, 172 BPM). Os packs reais aparecem ao lado dele.

## 1. Instalar o Python (uma vez)

```
pip install -r tools/packs/requirements.txt
```

## 2. Descarregar os samples
- As fontes sugeridas e as licenças estão em `docs/01-PESQUISA.md`, secção e).
- Põe cada pack descarregado numa pasta, por exemplo `C:/Samples/SampleRadar DnB`.
- Podes juntar várias pastas no mesmo pack, desde que tenham o mesmo BPM.
- **Vocais/MC:** podem vir de qualquer fonte, numa pasta à parte, passada com `--vocals`.
  - O script estima o tom de cada um e marca os que são "neutros" (sem tom claro: gritos, chops), que servem em qualquer pack.
  - Os loops vocais têm de ter o BPM do pack; os one-shots vocais não.

## 3. Analisar e sugerir os slots

```
python tools/packs/orimbux_pack.py scan --id dnb-essentials --name "DnB Essentials" --genre "Drum & Bass" "C:/Samples/SampleRadar DnB" --vocals "C:/Samples/Vocais"
```

- `--genre`: "Drum & Bass", "Neurofunk", "Jungle" ou "Liquid" (define a cor do pack).
- `--bpm 172`: opcional. Sem ele, o script deteta o BPM pelos nomes dos ficheiros e pela análise.
- `--cover imagem.png`: opcional.

O script escreve `packs-work/<id>/slots.txt`.

## 4. Confirmar a atribuição

Abre o `slots.txt`. Cada linha é um slot (`pad.tipo = ficheiro`):

| Slot | One-shot | Loop |
|---|---|---|
| `cross` ✕ | Kick | Break de bateria |
| `square` □ | Sub hit / 808 | Baixo (reese / neuro) |
| `circle` ○ | Snare | Percussão |
| `triangle` △ | Crash / hats | Tops (hats, rides) |
| `down` ↓ | Stab de acordes | Pads / acordes |
| `left` ← | Stab de synth | Lead / melodia |
| `right` → | Vocal chop / grito MC | Loop vocal |
| `up` ↑ | Riser / impacto / sirene | Atmosfera / FX |

- Troca os caminhos se quiseres (no fim do ficheiro há alternativas). Deixa vazio para um slot sem som.
- Opções depois de `;`:
  - `bars = 4` fixa o número de compassos (0 = automático);
  - `gain_db = -3` ajusta o volume;
  - `label = Amen` dá um nome curto ao som.

## 5. Gerar o pack

```
python tools/packs/orimbux_pack.py build packs-work/dnb-essentials/slots.txt
```

O script:
- converte para WAV 48 kHz 16 bits estéreo e normaliza a −1 dBFS;
- corta os loops em compassos exatos ao BPM, mais 5 ms de cauda (a app faz o crossfade no ponto de loop, sem cliques), com micro-fades anti-clique;
- nos one-shots, corta o silêncio do início, faz um fade-out de 5 ms e limita a 8 s;
- gera o `pack.json` e copia tudo para `app/src/main/assets/packs/<id>/`.

`--stretch`: se um loop vier noutro BPM, acerta-o no PC (offline). A app continua sem time-stretch.

## 6. Recompilar e instalar

```
.\tools\build-apk.ps1
.\tools\install-apk.ps1 192.168.x.x:PORTA
```

## Formato do `pack.json`

```json
{
  "id": "dnb-essentials",
  "name": "DnB Essentials",
  "genre": "Drum & Bass",
  "bpm": 172,
  "key": "Fá menor",
  "color": "#8B0000",
  "cover": "cover.png",
  "slots": [
    { "pad": "cross", "type": "oneshot", "file": "cross_oneshot.wav", "gain_db": 0, "label": "Kick 01" },
    { "pad": "cross", "type": "loop", "file": "cross_loop.wav", "bars": 2, "gain_db": 0, "label": "Amen" }
  ]
}
```

## Sons gravados pelo micro
- **Create (segurar) + pad:** grava até 4 s pelo micro do comando. Corta o silêncio, normaliza e substitui esse one-shot no pack atual.
- **O som fica guardado:** fica no telemóvel e sobrevive a reinícios.
- **"Meus sons":** no aviso que aparece, ou em Definições → Sons gravados, podes guardá-lo nesse pack.
- **Para voltar ao som do pack:** Definições → Sons gravados → Repor original.
