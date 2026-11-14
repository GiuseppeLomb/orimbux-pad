# Orimbux Pad — Especificação

Sampler para atuar ao vivo (estilo Pioneer DJS), para Android, controlado 100% por um comando PS5 DualSense ligado **por cabo USB-C**.
É uma versão final pronta a testar, não um protótipo: cada funcionalidade ou funciona bem ou sai da app (sem botões mortos).

## Alterações a esta especificação

| Data | Alteração |
|---|---|
| 2026-11-14 | **Comando:** a camada do comando é o SDL3 (driver HIDAPI do PS5), como ficou decidido na pesquisa. |
| 2026-11-14 | **Saída de som:** o Raio-X testa e mede os cenários A, B e "com hub". A escolha final faz-se com os resultados. O Bluetooth fica fora (não cumpre os 15 ms). |
| 2026-11-14 | **Metrónomo:** deixa de usar o altifalante do comando. Passa a ser um **pulso de vibração em cada tempo** (mais forte no tempo 1), com intensidade ajustável e opção de desligar. |
| 2026-11-14 | **Vocais:** o script de packs aceita uma pasta extra de vocais/MC de qualquer fonte. Os sons gravados pelo microfone do comando podem ser guardados como one-shots de um pack. |
| 2026-11-14 | **Hub USB-C com PD:** passa a ser uma forma de ligação suportada, para carregar o telemóvel durante a atuação (se o Raio-X confirmar). |

---

## Dispositivo e ligação
- **Telemóvel:** Samsung Galaxy S25 Ultra (Android 15+, One UI). Só este telemóvel interessa.
- **Ligação:** cabo USB-C ↔ USB-C. O telemóvel é o host e alimenta o comando. Sem Bluetooth.
- **Opção:** um hub USB-C com carregamento PD entre o telemóvel e o comando, para carregar o telemóvel durante a atuação.
- **Repo:** GitHub pessoal GiuseppeLomb (nunca na conta da Lumina Digital). O repo é público, mas os packs de som não vão para o git.
- **Testes:** o comando ocupa a porta USB, por isso o ADB é por Wi-Fi (ver `docs/02-RAIO-X.md`).

## Ordem de trabalho
1. Pesquisa — ✅ aprovada (`docs/01-PESQUISA.md`).
2. Raio-X — APK de diagnóstico, que fica depois dentro da app final (em Definições). ← **agora**
3. App completa.
4. Checklist de teste.

## Stack
- Kotlin + Jetpack Compose (interface). C++ via NDK (motor de áudio, análise do som para as luzes).
- **Comando:** SDL3 (zlib), driver HIDAPI do PS5, por cima da USB Host API do Android.
- **Áudio:** Oboe (baixa latência, modo exclusivo, 48 kHz). Meta: < 15 ms do botão ao som.
- Tudo offline, grátis, sem contas, sem anúncios, sem internet.
- Ecrã na horizontal, tema escuro, ecrã sempre ligado durante a atuação.

## Saída de som (a decidir com o Raio-X)
- **Cenário A:** música nos altifalantes do telemóvel + háptica nos canais 3–4 do comando, ao mesmo tempo (2 streams, cada um com o seu `setDeviceId`).
- **Cenário B:** música no jack 3,5 mm do comando (canais 1–2) + háptica (canais 3–4), num só stream de 4 canais.
- **Com hub:** os cenários A e B com o comando ligado por um hub USB-C com carregamento PD.
- O Raio-X mede a latência botão→som de cada cenário e regista o que funcionou. A app final oferece só os cenários que passarem.
- O altifalante do comando não é usado.

## Conceito
Sampler para atuar ao vivo, não para produzir: escolhes um pack, os loops entram e saem certos no compasso, disparas one-shots e controlas os efeitos com o comando. Sem sequenciador, sem edição, sem mistura de projetos.
O BPM é o do pack (sem time-stretch na app). Todos os packs ficam entre 160 e 175 BPM.

## Packs
- 8 packs pré-instalados: 2 de Drum & Bass, 2 de Neurofunk, 2 de Jungle, 2 de Liquid (fontes e licenças em `docs/01-PESQUISA.md`).
- Cada pack tem 16 sons: 8 loops + 8 one-shots, na estrutura fixa da tabela "Os 8 pads".
- Formato: `assets/packs/<id>/pack.json` + ficheiros WAV a 48 kHz.
  - `pack.json` guarda: nome, género, BPM, tom, cor do pack, capa (opcional) e, para cada um dos 16 slots: ficheiro, tipo (loop/one-shot), número de compassos (loops) e ganho.
- **Script no PC (Python)** que pega nas pastas de samples descarregadas e:
  - converte para WAV 48 kHz, normaliza, corta os loops em compassos exatos ao BPM (com micro-fades anti-clique);
  - deteta o BPM e sugere a atribuição de cada slot pelo nome e por análise espectral;
  - **aceita uma pasta extra de vocais/MC de qualquer fonte**. Usa sons com o mesmo tom/BPM do pack ou sons neutros (gritos, chops sem tom). Os loops vocais têm de ter o BPM do pack; os one-shots vocais não;
  - gera o `pack.json` e copia para `assets/packs`.
  - Tu confirmas a atribuição dos slots num ficheiro simples antes de gerar.
- Os loops repetem sem cliques (crossfade de 5 ms no ponto de loop).
- **Sons gravados pelo microfone do comando** (ver Create + pad): podem substituir um one-shot do pack atual ou ser guardados como one-shots de um pack — o pack "Meus sons" ou outro à escolha. Ficam guardados no telemóvel e sobrevivem a reinícios.

## Ergonomia (regra base do mapeamento)
- Só 4 dedos trabalham: 2 polegares + 2 indicadores. Médios, anelares e mindinhos seguram o comando.
- Os indicadores descansam em L1/R1 (para segurar) e descem a L2/R2 (para pressão).
- Nenhum gesto frequente obriga um polegar a estar em dois sítios ao mesmo tempo. Máximo 3 dedos em simultâneo.
- O giroscópio funciona como uma mão extra e só fica ativo enquanto se segura R1.
- Posição de descanso: polegar direito nos botões ✕○□△, polegar esquerdo no stick esquerdo.
- Mão direita = precisão rítmica. Mão esquerda = gestos lentos (segurar, varrer).

## Os 8 pads (camada base = one-shot; com L1 seguro = loop)

| Posição | One-shot (base) | Loop (L1 + pad) | Cor |
|---|---|---|---|
| ✕ | Kick | Break de bateria | Vermelho escuro #8B0000 |
| □ | Sub hit / 808 | Baixo (reese / neuro) | Violeta escuro #3A0CA3 |
| ○ | Snare | Percussão | Laranja #FF6B00 |
| △ | Crash / hats | Tops (hats, rides) | Amarelo claro #FFE66D |
| ↓ | Stab de acordes | Pads / acordes | Verde #2D9D5C |
| ← | Stab de synth | Lead / melodia | Azul ciano #00B4D8 |
| → | Vocal chop / grito MC | Loop vocal | Magenta #E0218A |
| ↑ | Riser / impacto / sirene | Atmosfera / FX | Branco gelo #E8F6FF |

As cores podem ser ajustadas. Regra: graves = tons escuros, agudos = tons claros.

Comportamento:
- **One-shots:** disparo imediato, sem quantização. O mesmo pad pode sobrepor-se (máximo de 4 vozes por pad; a mais antiga corta).
- **Loops:** L1 + pad liga/desliga, quantizado ao compasso seguinte. O pad pisca no ecrã enquanto espera para entrar ou sair.

## Mapeamento completo

### Polegar direito
- ✕ ○ □ △ → one-shots da tabela.
- **Stick direito** → corta grupos enquanto empurras (momentâneo, volta com a mola). Corte suave (fade de 5 ms). Zona morta de 30%.
  - ↑ agudos (△ + ↑)
  - ↓ baixo (□ + ↓ sub)
  - ← bateria (✕ + ○)
  - → melodia (↓ ← →)

### Indicador direito
- **R2 (pressão) sozinho** → BEAT ROLL da música toda. **Com um pad seguro** → NOTE REPEAT só desse som (o snare roll do build-up).
  - A pressão escolhe a divisão: 15–40% = 1/4, 40–65% = 1/8, 65–85% = 1/16, 85–100% = 1/32. Há histerese para não saltar entre valores.
  - O roll fica sincronizado à grelha do BPM. O gatilho R2 fica em modo vibração/weapon, com um clique sentido a cada repetição.
- **R1 (segurar) + inclinar o comando** → efeitos. O giroscópio só atua com R1 seguro; ao largar, o efeito desvanece em 200 ms.
  - Para a frente = echo/delay sincronizado (1/8 ponteado). Para trás = reverb. Rodar para a esquerda = flanger. Rodar para a direita = bitcrush.
  - O ângulo define a quantidade (0–45°). Zona morta de 5°. O ângulo de referência é a posição no momento em que se carrega no R1.

### Polegar esquerdo
- **Stick esquerdo (eixo X)** → filtro de DJ bipolar.
  - Esquerda = low-pass (abafa). Direita = high-pass (corta os graves). Centro = sem filtro.
  - Ressonância fixa e musical (Q ~0,9), com curva logarítmica. Zona morta de 10%. Ao largar, volta ao centro.
- **L3 (clique)** → bloqueia o filtro na posição atual, para o polegar poder ir às setas. Outro clique liberta e o filtro volta suavemente a 0. Aparece um ícone de bloqueio no ecrã.
- **Setas** → one-shots da tabela.

### Indicador esquerdo
- **L1 (segurar)** → camada de loops (Shift).
- **L2 (pressão)** → ECHO OUT: aumenta o envio para um delay com feedback alto e baixa o sinal seco. Ao largar, a cauda do eco continua naturalmente. O gatilho L2 fica em modo feedback, com resistência a crescer com a pressão.

### Fora da posição de jogo
- **Clique do touchpad** → DROP. Quantizado ao próximo compasso: silencia todos os loops durante 1 compasso e volta tudo no tempo 1 (os one-shots continuam a tocar durante o silêncio).
- **Deslizar no touchpad (vertical)** → volumes. Metade esquerda = volume dos loops, metade direita = volume dos one-shots. O valor fica onde o deixas; aceita 2 dedos.
- **Golpe rápido de rotação (yaw)** → REWIND estilo jungle. Só funciona com R1 solto e tem um limiar alto para não disparar por acidente.
  - Efeito de vinil a travar e a andar para trás (~1 tempo), silêncio até ao próximo tempo 1, e os loops recomeçam do início da frase.
- **Create (toque)** → começa/pára a gravação do set em WAV (`Música/OrimbuxPad/`). Um ponto vermelho no ecrã e na lightbar indica que está a gravar.
- **Create (segurar) + pad** → grava pelo microfone do comando enquanto seguras (máx. 4 s).
  - Corta o silêncio, normaliza e substitui esse one-shot.
  - Opção de guardar como one-shot de um pack ("Meus sons" ou outro).
- **Options** → ecrã de packs. A troca de pack acontece no próximo tempo 1, sem cortar o som de forma brusca.
- **PS** → recomeça todos os loops no tempo 1 (quantizado).
- **Mute** → mute geral, com o mute LED aceso.

**Modo canhoto (Definições):** espelha o mapa todo. Os sons e funções da direita passam para a esquerda e vice-versa (face↔setas, R1/R2↔L1/L2, sticks trocados).

## Luzes (fluidas e a acompanhar a música)
Análise em tempo real no motor de áudio (C++), atualizada a 60 Hz:
- **Cor da lightbar** = mistura das cores dos instrumentos que estão a tocar, pesada pelo volume (RMS) de cada um.
- **Tom** = claridade definida pelo centroide espectral do master: dominam os graves → tons escuros e profundos; dominam os agudos → tons claros.
- **Intensidade** = envelope de energia do master (ataque 10 ms, release 250 ms).
- **One-shot** = flash na cor do instrumento, que desvanece em ~300 ms por cima da cor base.
- Transições sempre suaves (interpolação no espaço de cor, sem saltos nem tremidos).
- **Player LEDs:** frase de 16 compassos. Os 4 LEDs exteriores acendem-se um a cada 4 compassos; o LED central pisca a cada tempo (mais forte no tempo 1).
- **Estados especiais:**
  - a gravar = ponto vermelho a pulsar por cima;
  - filtro bloqueado = respiração lenta;
  - Drop = as luzes apagam-se e voltam num flash no tempo 1.
- O ecrã usa as mesmas cores (os pads brilham com o som de cada um).
- Definições: intensidade global das luzes.

## Vibração e gatilhos
- **Háptica da música:** se o Raio-X confirmar a háptica por áudio USB, o kick + o baixo (low-pass a 150 Hz) vão para os canais hápticos 3–4, para se sentir a música nas mãos.
  - Se não confirmar: rumble gerado pelos transientes do kick e pelo envelope do baixo.
- **Metrónomo por vibração:** um pulso curto em cada tempo, pela mesma via da háptica (canais 3–4 ou rumble).
  - Força crescente: tempo normal < tempo 1 do compasso < tempo 1 da frase de 16 compassos (pulso forte).
  - Intensidade ajustável e opção de desligar nas Definições (ligado por defeito, a 50%).
  - O pulso está alinhado com o som que sai (compensado pela latência medida da saída).
- **Gatilhos:**
  - L2 = resistência progressiva (feedback);
  - R2 = clique a cada repetição do roll;
  - quando não estão em uso, ficam em modo off.
- Definições: intensidade da vibração.

## Ecrãs
1. **Packs** — grelha de capas na cor do pack, com nome, género e BPM. Navega-se com o comando (setas + ✕) e por toque.
2. **Atuação** — os 8 pads dispostos como no comando: cruz das setas à esquerda, losango ✕○□△ à direita, com cores e nome do som. Loops ativos acesos, loops em espera a piscar.
   - Também mostra:
     - barra de compasso e de frase de 16, e BPM;
     - posição do filtro (e bloqueio);
     - efeito ativo e quantidade, divisão do roll;
     - volumes dos loops/one-shots;
     - indicador de gravação;
     - estado e bateria do comando.
3. **Definições** — modo canhoto, intensidade das luzes, intensidade da vibração, metrónomo por vibração (ligado/desligado + intensidade), sensibilidade do giroscópio e calibração do ponto neutro, calibração da latência, saída de som (os cenários que passaram no Raio-X), Raio-X.

## Robustez
- Se o cabo sair: a música continua, aparece um aviso e o comando religa automaticamente quando voltar.
- Ao sair da app: o comando é reposto (luzes, gatilhos e rumble em off).
- Sem cliques nem estalidos em nenhuma transição (fades curtos em tudo).
- O pedido de permissão USB acontece só uma vez (dispositivo memorizado).

## Regras de trabalho
- Paragem em cada ✋ à espera de aprovação.
- Se algo for impossível neste telemóvel: diz-se logo, a funcionalidade sai e propõe-se a melhor alternativa. Nunca se finge que funciona.
- APK de debug instalável, com instruções em linguagem simples.
- Código comentado. README em português com o mapa do comando e como adicionar packs.
- Sem calendários nem estimativas de tempo.
