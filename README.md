# Orimbux Pad

Sampler para atuar ao vivo (Drum & Bass, Neurofunk, Jungle, Liquid) para Android, tocado 100% com um comando **PS5 DualSense ligado por cabo USB-C**.
Feito para o Samsung Galaxy S25 Ultra. Offline, grátis, sem contas, sem anúncios.

> **Estado:** ponto 2 de 4 — **Raio-X** (APK de diagnóstico). A app completa vem no ponto 3.
> Especificação: [docs/00-ESPECIFICACAO.md](docs/00-ESPECIFICACAO.md) · Pesquisa: [docs/01-PESQUISA.md](docs/01-PESQUISA.md) · Raio-X: [docs/02-RAIO-X.md](docs/02-RAIO-X.md)

## Mapa do comando

Só trabalham 4 dedos: os 2 polegares e os 2 indicadores. Os outros seguram o comando.

### Os 8 pads (base = one-shot · com L1 seguro = loop)

| Botão | One-shot | Loop (L1 + botão) |
|---|---|---|
| ✕ | Kick | Break de bateria |
| □ | Sub hit / 808 | Baixo (reese / neuro) |
| ○ | Snare | Percussão |
| △ | Crash / hats | Tops (hats, rides) |
| ↓ | Stab de acordes | Pads / acordes |
| ← | Stab de synth | Lead / melodia |
| → | Vocal chop / grito MC | Loop vocal |
| ↑ | Riser / impacto / sirene | Atmosfera / FX |

### Controlos

| Comando | Função |
|---|---|
| Stick direito (empurrar) | Corta grupos: ↑ agudos · ↓ baixo · ← bateria · → melodia |
| R2 (pressão) | BEAT ROLL de tudo; com um pad seguro = NOTE REPEAT desse som (1/4 → 1/32 com a pressão) |
| R1 + inclinar | Efeitos: frente = echo · trás = reverb · esquerda = flanger · direita = bitcrush |
| Stick esquerdo (X) | Filtro de DJ: esquerda = low-pass · direita = high-pass |
| L3 | Bloqueia / liberta o filtro |
| L1 (segurar) | Camada de loops |
| L2 (pressão) | ECHO OUT |
| Clique do touchpad | DROP (1 compasso de silêncio nos loops) |
| Touchpad (deslizar) | Volume: metade esquerda = loops · metade direita = one-shots |
| Rotação rápida (yaw) | REWIND estilo jungle |
| Create (toque) | Grava o set em WAV |
| Create (segurar) + pad | Grava pelo micro do comando para esse pad |
| Options | Ecrã de packs |
| PS | Recomeça os loops no tempo 1 |
| Mute | Mute geral |

Há também um **modo canhoto** nas Definições, que espelha o mapa todo.

## Compilar

Precisas do JDK 17+ e do Android SDK (com NDK 28.2 e CMake 3.31.6).

```powershell
.\tools\build-apk.ps1                      # descarrega o SDL3, compila e copia para apk\
.\tools\install-apk.ps1 192.168.x.x:PORTA  # instala por ADB sem fios
```

Também abre no Android Studio, mas corre primeiro `.\tools\fetch-deps.ps1`.
As instruções completas de instalação e teste estão em [docs/02-RAIO-X.md](docs/02-RAIO-X.md).

## Adicionar packs

Os packs **não vão para o git**, por causa das licenças "royalty-free, sem redistribuição". Ficam só na tua build local, em:

```
app/src/main/assets/packs/<id>/pack.json
app/src/main/assets/packs/<id>/*.wav      (48 kHz)
```

O `pack.json` guarda:
- nome, género, BPM (160–175), tom, cor e capa (opcional);
- para cada um dos 16 slots: ficheiro, tipo (loop/one-shot), número de compassos e ganho.

O script do PC que converte, corta, analisa e gera os packs (com a pasta extra de vocais/MC) chega no ponto 3.
O passo a passo vai ficar aqui.

## Estrutura

```
app/src/main/cpp/          motor nativo (C++)
  controller.*             camada do comando por cima do SDL3
  trigger_effects.*        efeitos dos gatilhos adaptativos
  audio_engine.*           áudio Oboe + medição de latência
app/src/main/java/pt/orimbux/pad/
  SdlBootstrap.kt          arranque do SDL3 numa app Compose
  raiox/                   ecrãs e lógica do Raio-X
tools/                     scripts (dependências, compilar, instalar)
docs/                      especificação, pesquisa, Raio-X
```

## Créditos e licenças

| Componente | Licença | Uso |
|---|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | zlib | comando DualSense (descarregado por `tools/fetch-deps.ps1`) |
| [Oboe](https://github.com/google/oboe) | Apache-2.0 | áudio de baixa latência |
| TriggerEffectGenerator — John "Nielk1" Klein | MIT | formato dos efeitos dos gatilhos (reescrito em C++) |
| Linux `hid-playstation.c` | GPL-2.0 | só referência dos reports (nenhum código copiado) |
