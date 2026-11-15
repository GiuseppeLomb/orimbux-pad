# Orimbux Pad

Sampler para atuar ao vivo (Drum & Bass, Neurofunk, Jungle, Liquid) para Android, tocado 100% com um comando **PS5 DualSense ligado por cabo USB-C**.
Feito para o Samsung Galaxy S25 Ultra. Offline, grátis, sem contas, sem anúncios.

> **Estado:** app completa (v1.0), pronta a testar. Checklist: [docs/04-CHECKLIST.md](docs/04-CHECKLIST.md)
> Especificação: [docs/00-ESPECIFICACAO.md](docs/00-ESPECIFICACAO.md) · Pesquisa: [docs/01-PESQUISA.md](docs/01-PESQUISA.md) · Raio-X: [docs/02-RAIO-X.md](docs/02-RAIO-X.md) · Packs: [docs/03-PACKS.md](docs/03-PACKS.md)

## Som e vibração
- **Saída principal: o comando.** A música sai no jack 3,5 mm do comando e a háptica nos atuadores, no mesmo stream USB.
  - O Android do S25 não deixa usar a saída de áudio do comando, por isso a app envia o áudio diretamente por USB (isócrono, sem root).
  - Latência medida do botão ao som: ~8 ms.
- **Alternativa:** altifalantes do telemóvel (Definições).
- **Se o cabo sair:** a música continua nos altifalantes, e volta ao comando quando o cabo é religado.
- **Vibração:** segue o volume de cada som, até 40% por defeito. Inclui o kick + o baixo, um impulso em cada one-shot e o metrónomo (mais forte no tempo 1).
- **Dinâmica:** a força da batida num pad (medida pelo acelerómetro) define o volume e a vibração do one-shot.

## Mapa do comando (destro)

Só trabalham 4 dedos: os 2 polegares e os 2 indicadores.

### Os 8 pads (base = one-shot · com L1 seguro = loop)

| Botão | One-shot | Loop (L1 + botão) | Modo por defeito |
|---|---|---|---|
| ✕ | Kick | Break de bateria | one-shot |
| □ | Sub hit / 808 | Baixo (reese / neuro) | one-shot |
| ○ | Snare | Percussão | one-shot |
| △ | Crash / hats | Tops (hats, rides) | one-shot |
| ↓ | Stab de acordes | Pads / acordes | gate |
| ← | Stab de synth | Lead / melodia | gate |
| → | Vocal chop / grito MC | Loop vocal | gate |
| ↑ | Riser / impacto / sirene | Atmosfera / FX | gate |

**Gate** = toca enquanto seguras (largar = fade-out de 15 ms). **One-shot** = toca até ao fim. Muda-se por pad nas Definições.

### Controlos

| Comando | Função |
|---|---|
| Stick direito (empurrar) | Corta grupos: ↑ agudos (△ ↑) · ↓ baixo (□) · ← bateria (✕ ○) · → melodia (↓ ← →) |
| R2 (pressão) | BEAT ROLL de tudo; com um pad seguro = NOTE REPEAT desse som. 15–40% = 1/4 · 40–65% = 1/8 · 65–85% = 1/16 · 85–100% = 1/32 |
| R1 + inclinar | Efeitos: frente = echo · trás = reverb · esquerda = flanger · direita = bitcrush (0–45°) |
| Stick esquerdo (X) | Filtro de DJ: esquerda = low-pass · direita = high-pass |
| L3 | Bloqueia / liberta o filtro |
| L1 (segurar) | Camada de loops (liga/desliga no compasso seguinte) |
| L2 (pressão) | ECHO OUT (gatilho com resistência progressiva) |
| Clique do touchpad | DROP: 1 compasso de silêncio nos loops, volta no tempo 1 |
| Touchpad (deslizar na vertical) | Volume: metade esquerda = loops · metade direita = one-shots |
| Rotação rápida (yaw) | REWIND estilo jungle (só com R1 solto) |
| Create (toque) | Começa/pára a gravação do set (Música/OrimbuxPad/) |
| Create (segurar) + pad | Grava pelo micro do comando para esse pad (máx. 4 s) |
| Options | Ecrã de packs (a troca acontece no tempo 1 seguinte) |
| PS | Recomeça os loops no tempo 1 |
| Mute | Mute geral (com o mute LED aceso) |

**Modo canhoto** (Definições): espelha o mapa todo (losango↔setas, R1/R2↔L1/L2, sticks trocados).

## Luzes
- **Lightbar:** a cor mistura os instrumentos que estão a tocar, pesada pelo volume. O tom fica escuro nos graves e claro nos agudos, e a intensidade segue a energia. Cada one-shot dá um flash na sua cor.
- **Player LEDs:** mostram a frase de 16 compassos. Os 4 exteriores enchem um a cada 4 compassos e o central pisca no tempo.
- **Estados:** a gravar = pulso vermelho; filtro bloqueado = respiração lenta; drop = apaga e volta num flash.

## Compilar

Precisas do JDK 17+ e do Android SDK (com NDK 28.2 e CMake 3.31.6).

```powershell
.\tools\build-apk.ps1                      # descarrega o SDL3, compila e copia para apk\
.\tools\install-apk.ps1 192.168.x.x:PORTA  # instala por ADB sem fios
```

Também abre no Android Studio, mas corre primeiro `.\tools\fetch-deps.ps1`. Como ativar o ADB por Wi-Fi: [docs/02-RAIO-X.md](docs/02-RAIO-X.md).

## Adicionar packs

Os packs **não vão para o git**, por causa das licenças "sem redistribuição". O script do PC converte, corta e analisa as tuas pastas de samples, incluindo uma pasta extra de vocais/MC:

```
pip install -r tools/packs/requirements.txt
python tools/packs/orimbux_pack.py scan --id dnb-essentials --name "DnB Essentials" --genre "Drum & Bass" "C:/Samples/DnB" --vocals "C:/Samples/Vocais"
# confirma packs-work/dnb-essentials/slots.txt e depois:
python tools/packs/orimbux_pack.py build packs-work/dnb-essentials/slots.txt
```

Passo a passo e formato do `pack.json`: [docs/03-PACKS.md](docs/03-PACKS.md).

## Estrutura

```
app/src/main/cpp/
  perf_engine.*     motor de atuação (transporte, pads, loops, efeitos, roll, háptica, análise)
  perf_input.*      mapeamento do comando, luzes e gatilhos (thread do SDL)
  perf_output.*     saídas (comando por USB direto / altifalantes) e gravação do set
  usb_direct.*      áudio USB isócrono para o comando (usbfs, sem root)
  controller.*      camada do comando por cima do SDL3 (HIDAPI PS5)
  pack.* demo_pack.cpp wav.* dsp.h trigger_effects.*
app/src/main/java/pt/orimbux/pad/
  ui/               ecrãs: packs, atuação, definições
  perf/             ponte para o motor, packs, definições, gravações
  raiox/            Raio-X (dentro das Definições)
tools/              dependências, compilar, instalar, criador de packs (Python)
docs/               especificação, pesquisa, Raio-X, packs, checklist
```

## Créditos e licenças

| Componente | Licença | Uso |
|---|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | zlib | comando DualSense (descarregado por `tools/fetch-deps.ps1`) |
| [Oboe](https://github.com/google/oboe) | Apache-2.0 | áudio de baixa latência (modo altifalantes) |
| TriggerEffectGenerator — John "Nielk1" Klein | MIT | formato dos efeitos dos gatilhos (reescrito em C++) |
| Freeverb — Jezar at Dreampoint | domínio público | algoritmo do reverb (reescrito) |
| RBJ Audio EQ Cookbook | domínio público | fórmulas dos filtros |
| Linux `hid-playstation.c` | GPL-2.0 | só referência dos reports (nenhum código copiado) |
