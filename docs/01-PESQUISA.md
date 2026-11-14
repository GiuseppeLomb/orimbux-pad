# Orimbux Pad — Ponto 1: Pesquisa (referência técnica)

Data: 2026-11-14 · Alvo: Samsung Galaxy S25 Ultra + DualSense (054C:0CE6) por cabo USB-C.

Este ficheiro guarda os detalhes técnicos (bytes, links, licenças) para os pontos seguintes.
As decisões e os veredictos estão resumidos no fim.

---

## a) SDL3 (driver HIDAPI do PS5) no Android por USB

**Veredicto: SIM — o SDL3 cobre tudo o que é pedido.**

Como funciona no Android:
- O SDL tem uma camada Java própria (`HIDDeviceManager.java` + `HIDDeviceUSB.java`) que usa a USB Host API do Android.
- Aceita qualquer interface de classe HID (sem filtro de marca). O DualSense tem a interface HID separada das interfaces de áudio.
- Abre o dispositivo e faz `claimInterface(iface, true)` (force = true) **só na interface HID**. O áudio USB continua com o Android.
- Lê os input reports numa thread Java com `bulkTransfer(..., 1000 ms)` e escreve os output reports com `bulkTransfer` no endpoint OUT.
- O USB está ligado sem precisar de hints (`hid_init` chama `initialize(usb=true, bluetooth=false)`). O Bluetooth só serve para o Steam Controller (não interessa: só usamos cabo).
- Pedido de permissão USB: `requestPermission` com `PendingIntent` `FLAG_MUTABLE` (Android 12+). Se a app já tiver permissão (via filtro `USB_DEVICE_ATTACHED` com "Abrir sempre"), não aparece diálogo.
- Commit recente: o SDL liberta o comando quando a app vai para segundo plano e volta a reclamá-lo ao regressar.

O que o driver PS5 (`SDL_hidapi_ps5.c`) dá:

| Função | Suporte | Como |
|---|---|---|
| Botões (incl. PS, Create, Options, Mute, L3, R3, clique do touchpad) | ✅ | API normal de gamepad |
| 2 sticks, L2/R2 analógicos | ✅ | eixos |
| Touchpad 2 dedos | ✅ | `SDL_AddTouchpad(…, 2)` → `SDL_GetGamepadTouchpadFinger` |
| Giroscópio + acelerómetro | ✅ | calibrados com o feature report 0x05; **250 Hz por USB** |
| Lightbar RGB | ✅ | `SDL_SetGamepadLED` |
| Player LEDs | ✅ | padrões por jogador; padrão livre dos 5 LEDs via efeito raw |
| Mute LED | ✅ (raw) | via `SDL_SendGamepadEffect` |
| Rumble | ✅ | modo "melhorado" se firmware ≥ 0x0224; senão emulação |
| Gatilhos adaptativos | ✅ (raw) | via `SDL_SendGamepadEffect` |
| Bateria | ✅ | `SDL_GetGamepadPowerInfo` |

- `SDL_SendGamepadEffect(data, size)` copia os bytes que lhe damos para o output report USB 0x02 (48 bytes, payload a partir do byte 1). Assim controlamos gatilhos, mute LED, padrão dos LEDs, volumes e o caminho de áudio (altifalante/jack).
- O driver **não reenvia efeitos periodicamente** — só quando a app pede. Não há conflito.
- Latência de entrada: report a 250 Hz → atraso do comando entre 0 e 4 ms (média ~2 ms).

Ressalvas:
1. O SDL3 no Android foi feito para correr dentro da `SDLActivity`. Numa app Kotlin/Compose temos de arrancar a parte Java à mão (`SDL.setupJNI()`, `SDL.initialize()`, `SDL.setContext()`, `HIDDeviceManager`). É a parte com mais atrito; valida-se no Raio-X.
2. A entrada passa por Java → JNI → fila do SDL → a nossa thread chama `SDL_UpdateGamepads`. Custo < 1 ms.
3. Plano B (se a integração falhar): a camada própria é pequena — os formatos estão todos em b).

---

## b) Formatos dos reports do DualSense por USB

Fonte principal: kernel Linux `drivers/hid/hid-playstation.c` (GPL-2.0 — só como referência, não se copia código), confirmado com o SDL3 e com o Nielk1 (gatilhos).

### Input report 0x01 (64 bytes, ~250 Hz)

| Byte | Conteúdo |
|---|---|
| 0 | Report ID = 0x01 |
| 1 / 2 | Stick esquerdo X / Y (0–255, centro ~128) |
| 3 / 4 | Stick direito X / Y |
| 5 / 6 | L2 / R2 analógicos (0–255) |
| 7 | contador de sequência |
| 8 | bits 0–3 setas (hat: 0 ↑, 1 ↗, 2 →, 3 ↘, 4 ↓, 5 ↙, 6 ←, 7 ↖, 8 solto) · bit4 □ · bit5 ✕ · bit6 ○ · bit7 △ |
| 9 | bit0 L1 · bit1 R1 · bit2 L2 · bit3 R2 · bit4 Create · bit5 Options · bit6 L3 · bit7 R3 |
| 10 | bit0 PS · bit1 clique do touchpad · bit2 Mute |
| 11 | (só DualSense Edge: Fn e pás) |
| 12–15 | reservado |
| 16–21 | giroscópio X, Y, Z (int16 LE) |
| 22–27 | acelerómetro X, Y, Z (int16 LE) |
| 28–31 | timestamp do sensor (uint32 LE) |
| 32 | reservado |
| 33–36 | toque 1 · byte 33: bit7 = 1 se **não** está a tocar, bits 0–6 = id · X = b34 \| (b35 & 0x0F) << 8 (0–1919) · Y = (b35 >> 4) \| b36 << 4 (0–1079) |
| 37–40 | toque 2 (mesmo formato) |
| 41–52 | reservado |
| 53 | bateria: bits 0–3 nível (% = nível×10+5, máx. 100) · bits 4–7 estado (0 descarga, 1 a carregar, 2 cheia, 0xA/0xB sem carga, 0xF erro) |
| 54 | deteção de auscultadores / microfone no jack (bits a confirmar no Raio-X) |
| 55–63 | estado / reservado |

### Output report 0x02 (USB, 48 bytes)

| Byte | Campo |
|---|---|
| 0 | 0x02 |
| 1 | flags0: bit0 rumble compatível · bit1 seleção háptica · bit2 efeito gatilho R2 · bit3 efeito gatilho L2 · bit4 volume auscultadores · bit5 volume altifalante · bit6 volume microfone · bit7 controlo de áudio |
| 2 | flags1: bit0 mute LED · bit1 power-save (mute do micro) · bit2 lightbar · bit3 libertar LEDs · bit4 player LEDs · bit7 audio_control2 |
| 3 | motor direito (fino) 0–255 |
| 4 | motor esquerdo (forte) 0–255 |
| 5 | volume auscultadores 0–0x7F |
| 6 | volume altifalante 0–0xFF |
| 7 | volume microfone 0–0x40 |
| 8 | audio_control: bits 4–5 caminho de saída (0 = auscultadores L/R e altifalante mudo · 3 = altifalante com o canal **R**, auscultadores mudos) |
| 9 | mute LED: 0 apagado · 1 aceso · 2 a pulsar |
| 10 | power_save_control: bit4 = mute do microfone |
| 11–21 | efeito do gatilho **R2** (11 bytes) |
| 22–32 | efeito do gatilho **L2** (11 bytes) |
| 33–37 | reservado |
| 38 | audio_control2: bits 0–2 ganho do pré-amp do altifalante (2 = +6 dB) |
| 39 | flags2: bit1 setup da lightbar · bit2 rumble melhorado (firmware ≥ 2.24) |
| 40–41 | reservado |
| 42 | lightbar_setup (bit1 = desligar a animação de arranque) |
| 43 | brilho dos player LEDs (0 alto · 1 médio · 2 baixo) |
| 44 | player LEDs: bits 0–4 = 5 LEDs da esquerda para a direita (o central é o bit 2) · bit5 = mudar sem fade |
| 45 / 46 / 47 | lightbar R / G / B |

Exemplo verificado (DroidDeck, em hardware): altifalante ligado = `[1]=0xA0 [2]=0x80 [6]=0x64 [8]=0x30 [38]=0x02`; auscultadores = `[1]=0x80 [8]=0x00`.

### Efeitos dos gatilhos (11 bytes; Nielk1 TriggerEffectGenerator, MIT)

Byte 0 = modo. As 10 zonas cobrem o curso do gatilho; força/amplitude 1–8 guardada como valor−1 em 3 bits por zona.

| Modo | Byte 0 | Bytes seguintes | Uso no Orimbux Pad |
|---|---|---|---|
| Off | 0x05 | tudo 0 | gatilho livre quando não está em uso |
| Feedback | 0x21 | 1–2 zonas ativas (LE) · 3–6 forças (3 bits/zona, LE) | **L2**: rampa de resistência (slope 1→8) |
| Weapon | 0x25 | 1–2 = (1<<início)\|(1<<fim) · 3 = força−1 | **R2**: "clique" sentido |
| Vibration | 0x26 | 1–2 zonas · 3–6 amplitudes · 9 = frequência (Hz) | **R2**: impulso curto em cada repetição do roll |

### Feature reports

| ID | Tamanho | Uso |
|---|---|---|
| 0x05 | 41 | calibração do gyro/acelerómetro |
| 0x09 | 20 | info de emparelhamento (MAC) |
| 0x20 | 64 | firmware (versão nos bytes 44–45) |

---

## c) Áudio USB do DualSense no Android

Hardware (confirmado em Windows/Linux):
- Dispositivo USB Audio Class. **Saída 4 canais, 48 kHz, 16 bits**: canais 1–2 = jack 3,5 mm (estéreo) e altifalante (mono, usa o canal R quando o caminho = 3) · canais 3–4 = atuadores hápticos.
- A háptica funciona **só com áudio nos canais 3–4** — não precisa de comando HID (demo WASAPI no Windows). Não se usa ao mesmo tempo que o modo rumble.
- Entrada: microfone (formato exato confirmado no Raio-X).

No Android (o ponto incerto):
- A documentação oficial de USB áudio (Android 5) fala só de mono/estéreo. Mas desde o Android 6 existem as *channel index masks*, feitas para interfaces USB com mais de 2 canais; a própria documentação do `AudioFormat` usa um dispositivo USB de 4 canais como exemplo (máscara 0xF). O Android 17 acrescenta testes CTS para saída USB multicanal.
- **Ninguém publicou um teste confirmado da háptica do DualSense num telemóvel Android.** Dois projetos Android open source implementaram em 2026 o mesmo método (AudioTrack com máscara 0xF + `setPreferredDevice` no comando): DroidDeck (PR fechado sem merge) e dish-android. Ambos dizem que não testaram em telemóvel.
- Logo: **o S25 Ultra só se confirma no Raio-X.**

Plano A — pelo Android (sem root):
- Stream de 4 canais (máscara 0xF) com o dispositivo USB do DualSense escolhido explicitamente (Oboe `setDeviceId`; se o Oboe não aceitar 4 canais nesse dispositivo, `AudioTrack` com `setChannelIndexMask(0xF)` + `setPreferredDevice`).
- Microfone: `AudioRecord` com `setPreferredDevice` na entrada USB do comando. Pedimos `RECORD_AUDIO` **antes** da permissão USB (o diálogo USB do Android trata de forma diferente apps sem permissão de gravação).
- Nota: com o comando ligado, o Android passa o som "por defeito" para o comando (é um headset USB). A nossa app escolhe a saída de cada stream explicitamente.

Plano B — libusb (LGPL-2.1):
- `libusb_wrap_sys_device()` sobre o fd do `UsbDeviceConnection`, com `LIBUSB_OPTION_NO_DEVICE_DISCOVERY`, e transferências isócronas na interface de streaming de áudio (reclamada com force → o Android deixa de ver o comando como placa de som).
- Possível sem root em princípio (as apps de câmaras UVC fazem isócronas assim), mas há relatos de problemas. É bastante mais complexo. Só se o plano A falhar e a háptica por áudio valer a pena.
- Se A e B falharem: rumble gerado pelos transientes do kick e pelo envelope do baixo (como está na especificação).

### ⚠ Por onde sai a música principal?
A única porta USB do telemóvel fica ocupada pelo comando. Opções:

| Saída | Latência esperada | Notas |
|---|---|---|
| Altifalantes do telemóvel | a mais baixa (AAudio em modo exclusivo) | som fraco para atuar |
| **Jack 3,5 mm do comando** (canais 1–2 do mesmo stream USB) | a medir no Raio-X | auscultadores ou cabo para PA ligados ao comando — ideal para atuar |
| Bluetooth | 100–250 ms típicos | **incompatível com a meta de 15 ms** → não se usa |

Conflito: se a música sair pelo jack, o altifalante do comando fica mudo (caminho 0) → o metrónomo no altifalante só funciona quando a música sai pelo telemóvel.

---

## d) Projetos open source reaproveitáveis

Não encontrei nenhum sampler Android open source controlado por DualSense. O que se aproveita:

| Projeto | Licença | Para quê |
|---|---|---|
| SDL3 | zlib | camada do comando (input, LEDs, rumble, efeitos raw) |
| Oboe + exemplo DrumThumper (`parselib` + `iolib`) | Apache-2.0 | base do motor: leitura de WAV, vozes de samples, mistura de baixa latência |
| Nielk1 TriggerEffectGenerator | MIT | codificação dos gatilhos (portar para C++) |
| Linux `hid-playstation.c` | GPL-2.0 | referência dos reports (sem copiar código) |
| DroidDeck PR #1339 · dish-android PR #210 | — | referência: 4 canais para o DualSense no Android (não testado em hardware) |
| Dualsense-Multiplatform (rafaelvaloto) | MIT | referência de háptica por áudio e gatilhos (C++20; **não suporta Android**) |
| dualsense-ts (nsfm) | — | referência de volumes, caminho de áudio, mute LED (WebHID/Node) |
| pd-dualsense (ben-wes) | Unlicense | referência de entradas/LEDs/gatilhos (macOS/Windows; sem háptica por áudio) |
| libusb | LGPL-2.1 | só no plano B (isócronas) |
| dr_wav | domínio público / MIT-0 | gravar o set em WAV |
| pffft ou KissFFT | BSD | FFT para o centroide espectral das luzes |
| Freeverb | domínio público | reverb |
| Script do PC: librosa (ISC), soundfile, numpy, pyloudnorm (MIT) | — | BPM, conversão, normalização |

Os efeitos (filtro bipolar, delay, flanger, bitcrush, echo out) escrevem-se de raiz (biquads RBJ, linhas de atraso) — são pequenos e assim não há dependências.

---

## e) Packs gratuitos (Drum & Bass, Neurofunk, Jungle, Liquid)

Quase todas as licenças são "royalty-free, proibido redistribuir os ficheiros": podes usá-los em atuações e gravações; **não podem ir para o git nem num APK partilhado** (encaixa no plano: ficam só na tua build local).

| # | Género | Pack | Editor | Conteúdo / BPM | Acesso | Licença |
|---|---|---|---|---|---|---|
| 1 | DnB | [475 Drum 'n' Bass Essentials](https://www.musicradar.com/news/sampleradar-drum-and-bass-essentials-samples) | SampleRadar | loops de bateria, one-shots, stabs, FX, baixos, melodias, airhorns | direto | royalty-free, sem redistribuir |
| 2 | DnB | [328 Drum 'n' Bass Samples](https://www.musicradar.com/news/tech/sampleradar-222244) | SampleRadar | 2 construction kits, loops a 170 BPM | direto | royalty-free (termos SampleRadar) |
| 3 | Neurofunk | [Neurofunk – Free Sample Pack](https://www.ghosthack.de/free_sample_packs/neurofunk-free-sample-pack) | Ghosthack | 70: bateria, 21 neuro basses, 14 reese, FX · 172 BPM | email | royalty-free |
| 4 | Neurofunk | [Free Neuro DnB Sample Pack](https://app.dnbacademy.net/free-neuro-dnb-sample-pack) | DNB Academy | 50: 25 neuro basses, bateria, FX | conta grátis | royalty-free |
| 5 | Jungle | [207 Free '90s Jungle Samples](https://www.musicradar.com/music-tech/sampleradar-207-free-90s-jungle-samples) | SampleRadar | breaks, baixos, one-shots, pads, strings | direto | royalty-free |
| 6 | Jungle | [Jungle Inspirations Vol.1](https://kansamples.com/blogs/free-drum-and-bass-sample-packs/jungle-inspirations-vol-1) | KAN Samples | 6 song starters + 28 loops + 12 MIDI | direto | descrito como royalty-free ⚠ |
| 7 | Liquid | [Free Liquid Drum and Bass Samples](https://www.ghosthack.de/free_sample_packs/free-liquid-drum-and-bass-samples) | Ghosthack | 44 loops/samples: bateria, baixo, melódicos | email | royalty-free (Ghosthack) |
| 8 | Liquid | [Free Liquid DnB Sample Pack](https://app.dnbacademy.net/free-liquid-dnb-sample-pack) | DNB Academy | 26: bateria, musicais, FX | conta grátis | royalty-free |

Reservas e complementos (para slots em falta, sobretudo pads ↓, atmosferas ↑ e FX):
- [Kulture Free DNB Beta Pack](https://kulturesamples.com/products/free-kulture-dnb-beta-pack) — 134 WAV, royalty-free, sem redistribuir, email.
- [Samplephonics Liquid DnB Freebie](https://www.noiiz.com/sounds/free_packs/730) — conta Noiiz grátis.
- [Ghosthack Free DnB Samples Vol. 2](https://eu.ghosthack.de/free_sample_packs/free-drum-and-bass-samples-vol.-2) — 100 DnB/neuro.
- SampleRadar: [130 pads DnB](https://www.musicradar.com/news/tech/sampleradar-449476) · [100 atmosferas DnB](https://www.musicradar.com/news/tech/sampleradar-429444) · [100 FX DnB](https://www.musicradar.com/news/tech/sampleradar-444650).
- Arachne (Loop Cult, 84 loops 172–174 BPM) — licença não declarada → só depois de confirmada.

A evitar: Breakform (Ghost Syndicate) e os "Break Tribute" da KAN — feitos a partir de discos antigos, **não são royalty-free**.

⚠ Breaks de jungle: muitos packs "royalty-free" usam breaks clássicos (Amen, Think…). Para tocar ao vivo não há problema prático; para lançar gravações comercialmente, convém confirmar.

Notas para o script do PC:
- Um pack gratuito raramente tem os 16 sons da tabela (o slot mais fraco é o vocal → / MC). O script deixa compor um pack a partir de várias fontes **com o mesmo BPM**.
- Opção a decidir no ponto 3: acertar o BPM de loops vindos de fontes diferentes **no PC, offline** (rubberband). A app continua sem time-stretch.
- Os packs da Ghosthack vêm a 44,1 kHz → o script converte para 48 kHz.

---

## ADB por Wi-Fi (S25 Ultra)

1. **Definições → Acerca do telemóvel → Informações de software** → toca 7 vezes em **Número de compilação** → mete o PIN. Aparece "Modo de programador ativado".
2. **Definições → Opções de programador** → liga **Depuração sem fios**. Telemóvel e PC na mesma rede Wi-Fi; aceita "Permitir nesta rede".
3. Toca no **texto** "Depuração sem fios" (não no interruptor) → **Emparelhar dispositivo com código de emparelhamento**. Aparecem um IP:porta e um código de 6 dígitos.
4. No PC: `adb pair 192.168.x.x:PORTA_DO_EMPARELHAMENTO` e escreve o código.
5. No PC: `adb connect 192.168.x.x:PORTA` — a porta que aparece no ecrã principal da Depuração sem fios (é **diferente** da do emparelhamento).
6. `adb devices` deve mostrar o telemóvel.

- O emparelhamento faz-se uma vez. A porta do passo 5 muda quando o Wi-Fi ou a depuração sem fios se desligam → repete só o passo 5.
- O `adb` vem nas "Android SDK Platform-Tools" da Google (verifico no PC no ponto 2).
- Instalar o APK: `adb install -r app-debug.apk`, ou copiar o APK para o telemóvel e abri-lo (permitir "Instalar apps desconhecidas" ao gestor de ficheiros).

---

## Limitações confirmadas / riscos

- **Bluetooth para a música**: impossível cumprir < 15 ms → fora.
- **Háptica por áudio no S25 Ultra**: desconhecida → decide-se no Raio-X (plano A → plano B → rumble).
- **Bateria do telemóvel**: o telemóvel alimenta e carrega o comando → gasta mais bateria. Com um só cabo não dá para carregar o telemóvel ao mesmo tempo.
- **Botão PS**: com a interface HID reclamada pela app, o PS deixa de abrir o ecrã inicial do Android (é o que queremos). Ao sair da app, o comando volta ao Android.
- **Latência**: comando 0–4 ms + motor de áudio. Pelos altifalantes do telemóvel a meta é realista; pelo jack do comando mede-se no Raio-X.

---

## Fontes

- SDL: [HIDDeviceManager.java](https://github.com/libsdl-org/SDL/blob/main/android-project/app/src/main/java/org/libsdl/app/HIDDeviceManager.java) · [HIDDeviceUSB.java](https://github.com/libsdl-org/SDL/blob/main/android-project/app/src/main/java/org/libsdl/app/HIDDeviceUSB.java) · [hid.cpp (Android)](https://github.com/libsdl-org/SDL/blob/main/src/hidapi/android/hid.cpp) · [SDL_hidapi_ps5.c](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_ps5.c) · [commit: libertar USB em segundo plano](https://discourse.libsdl.org/t/sdl-ensure-we-release-exclusive-usb-access-to-controllers-when-backgrounded-15694/68205)
- [Linux hid-playstation.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c)
- [Nielk1 TriggerEffectGenerator](https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db)
- [Android: USB digital audio](https://source.android.com/docs/core/audio/usb) · [AudioFormat (channel index masks)](https://developer.android.com/reference/android/media/AudioFormat) · [CTS multicanal USB](https://source.android.com/docs/compatibility/cts/multi-channel-usb-audio)
- [Wine: layout de 4 canais do DualSense](https://www.winehq.org/pipermail/wine-devel/2022-July/222742.html) · [Demo WASAPI canais 3–4](https://gist.github.com/DJm00n/f5e3062d54cc675827e5578f904efe0d)
- [DroidDeck PR #1339](https://github.com/Droid-Deck/DroidDeck/pull/1339) · [dish-android PR #210](https://github.com/TinkerNorth/dish-android/pull/210)
- [Dualsense-Multiplatform](https://github.com/rafaelvaloto/Dualsense-Multiplatform) · [dualsense-ts](https://cdn.jsdelivr.net/npm/dualsense-ts@6.15.0/README.md) · [pd-dualsense](https://github.com/ben-wes/pd-dualsense)
- [libusb no Android (README)](https://flutter.googlesource.com/third_party/libusb/+/a584d060c853c9867dabc915fbafcbeccba1bc12/android/README) · [SystemUI: diálogos de permissão USB áudio](https://android.googlesource.com/platform/frameworks/base/+/main/packages/SystemUI/docs/usb_audio.md)
- Packs: [KAN — 39 packs DnB grátis (2026)](https://kansamples.com/blogs/free-drum-and-bass-sample-packs/20-best-free-drum-bass-sample-packs-2025) · [Ghosthack DnB grátis](https://eu.ghosthack.de/Service/Free-Sample-Packs/Drum-and-Bass/) · links na tabela e).
