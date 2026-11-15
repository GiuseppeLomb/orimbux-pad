# Checklist de teste — Orimbux Pad 1.0

Marca cada linha ✅ ou ❌, e anota o que não estiver bem.

**Antes de começar:**
- ligar auscultadores (ou o cabo para o PA) ao jack do comando;
- ligar o comando ao telemóvel por cabo;
- abrir a app.

A app abre no ecrã de packs. Escolhe o **Pack de teste** (setas + ✕).

## A. Arranque e ligação
1. [ ] A app abre sem erros, na horizontal e em ecrã inteiro. O ecrã não se apaga sozinho.
2. [ ] No topo do ecrã de atuação aparece "Comando ligado · xx%" e "Comando · jack + háptica · fila de 8 ms". A taxa fica ≥ 240 Hz.
3. [ ] O som sai pelo jack do comando (não pelo telemóvel).
4. [ ] **Cabo fora:** a música continua nos altifalantes do telemóvel e aparece o aviso "Comando desligado".
5. [ ] **Cabo de volta:** o comando religa sozinho e o som volta ao jack. Não aparece outra vez o diálogo de permissão USB.
6. [ ] **Ao sair da app:** a lightbar, os player LEDs, os gatilhos e o rumble ficam desligados.

## B. Pads (destro)
7. [ ] ✕ □ ○ △ e ↓ ← → ↑ tocam os one-shots da tabela, logo, sem atraso.
8. [ ] Vários toques rápidos no mesmo pad sobrepõem-se (até 4). O mais antigo corta sem estalido.
9. [ ] **Dinâmica:** um toque fraco soa e vibra menos do que um forte, numa curva contínua, sem degraus. "Força da última batida" muda no ecrã.
10. [ ] **Gate:**
    - nas setas, um toque rápido dá um som curto e largar faz um fade-out sem clique;
    - no losango (one-shot), o som vai até ao fim.
11. [ ] **L1 + pad:** liga o loop no compasso seguinte. O pad pisca no ecrã enquanto espera e fica aceso quando entra.
12. [ ] **L1 + pad outra vez:** o loop sai no compasso seguinte, sem cliques.
13. [ ] Os loops repetem sem cliques no ponto de loop (ouvir 2–3 voltas do baixo e dos pads).

## C. Mão direita
14. [ ] **Stick direito:**
    - ↑ corta os agudos (△ e ↑);
    - ↓ corta o baixo (□);
    - ← corta a bateria (✕ e ○);
    - → corta a melodia (↓ ← →).
    Ao largar, tudo volta, sem estalidos.
15. [ ] **R2 sozinho** (pressão crescente): beat roll de toda a música, em 1/4 → 1/8 → 1/16 → 1/32. Fica certo no tempo e não salta entre divisões.
16. [ ] **R2 + pad seguro:** note repeat só desse som (o snare roll).
17. [ ] **Gatilho R2:** sente-se um clique a cada repetição (1/4 a 1/16) e vibração contínua em 1/32.
18. [ ] **R1 + inclinar para a frente:** echo. **Para trás:** reverb. **Rodar para a esquerda:** flanger. **Para a direita:** bitcrush. A quantidade cresce com o ângulo.
19. [ ] **Ao largar o R1:** o efeito desvanece (~200 ms).

## D. Mão esquerda
20. [ ] **Stick esquerdo:** para a esquerda abafa (low-pass), para a direita corta os graves (high-pass). No centro não há filtro.
21. [ ] **L3:** bloqueia o filtro (aparece 🔒 e a lightbar "respira"). **L3 outra vez:** o filtro volta suavemente ao centro.
22. [ ] **L2:** echo out. O seco baixa e o eco cresce; ao largar, a cauda continua. O gatilho L2 tem resistência a crescer com a pressão.

## E. Fora da posição de jogo
23. [ ] **Clique do touchpad:** DROP no compasso seguinte. Os loops calam-se durante 1 compasso e voltam no tempo 1; os one-shots continuam a tocar.
24. [ ] **Deslizar no touchpad:** a metade esquerda muda o volume dos loops e a direita o dos one-shots. O valor fica onde o deixas.
25. [ ] **Rotação rápida do comando (com R1 solto):** rewind. O vinil trava e anda para trás, há silêncio até ao tempo 1 e os loops recomeçam do início da frase.
26. [ ] **Create (toque):** começa a gravação (REC no ecrã + pulso vermelho na lightbar). **Outro toque:** pára. O ficheiro aparece em Música/OrimbuxPad/ e abre num leitor.
27. [ ] **Create (segurar) + pad, falar, largar:** o pad passa a tocar a gravação (silêncio cortado, normalizada).
28. [ ] **"Guardar em Meus sons":** o pack "Meus sons" aparece no ecrã de packs.
29. [ ] **Options:** abre o ecrã de packs. Ao escolher outro pack, a troca acontece no tempo 1, sem corte brusco.
30. [ ] **PS:** os loops recomeçam no tempo 1 seguinte.
31. [ ] **Mute:** mute geral, com o mute LED aceso. **Outra vez:** som e LED voltam.

## F. Luzes
32. [ ] **Lightbar:**
    - muda de cor com os instrumentos que tocam;
    - fica mais escura com graves e mais clara com agudos;
    - a intensidade acompanha a energia;
    - não há saltos nem tremidos.
33. [ ] Cada one-shot dá um flash na cor do pad (~300 ms).
34. [ ] **Player LEDs:** os 4 exteriores enchem um a cada 4 compassos (frase de 16) e o central pisca em cada tempo, mais tempo no tempo 1. Se encherem ao contrário, ativa "Inverter a ordem" nas Definições.
35. [ ] **Drop:** as luzes apagam e voltam num flash no tempo 1.
36. [ ] No ecrã, os pads brilham com o som de cada um.

## G. Vibração
37. [ ] Sente-se o kick e o baixo nas mãos, a seguir à música.
38. [ ] O impulso de cada one-shot sente-se ao mesmo tempo que o som; um som mais fraco vibra menos.
39. [ ] **Metrónomo:** um pulso em cada tempo, mais forte no tempo 1, e o mais forte no início de cada frase. Desliga-se e ajusta-se nas Definições.
40. [ ] A vibração não passa dos 40% por defeito. O cursor das Definições muda a intensidade.

## H. Definições
41. [ ] **Modo canhoto:** as setas passam a ser o losango (e vice-versa), R1/R2 ↔ L1/L2 e os sticks trocam. O ecrã mostra a nova disposição.
42. [ ] **Saída = Altifalantes do telemóvel:** a música sai no telemóvel e a háptica continua no comando. "Atraso da háptica" alinha as duas.
43. [ ] **Fila USB de 4 ms:** sem cortes durante 2–3 minutos. Se houver cortes, volta a 8 ms.
44. [ ] **Giroscópio:** a sensibilidade e "inverter frente/trás" mudam o comportamento. "Calibrar ponto neutro" (com o comando pousado) acaba com rewinds por acidente.
45. [ ] **Dinâmica:** depois de calibrar (5 fracos + 5 fortes), a resposta fica mais natural. Desligada, o volume fica fixo com uma pequena variação.
46. [ ] **Gate/one-shot por pad:** muda o comportamento do pad escolhido.
47. [ ] **Sons gravados → Repor original:** o pad volta ao som do pack.
48. [ ] **Raio-X:** abre, testa (incluindo "LED 1…5 sozinhos" e "Mostrar a frase") e, ao fechar, a atuação volta com som.
49. [ ] As definições ficam guardadas depois de fechar e voltar a abrir a app.

## I. Packs reais (quando os tiveres)
50. [ ] **Script de packs:** `scan` e depois `build` geram o pack. Depois de recompilar, ele aparece na grelha com a cor, o nome, o género e o BPM.
51. [ ] Os loops do pack real entram certos no compasso e repetem sem cliques.

## J. Estabilidade
52. [ ] 20 minutos a tocar sem cortes, sem estalidos e sem a app fechar.
53. [ ] A taxa de reports do comando (no topo) fica ≥ 240 Hz durante a atuação. Se cair (como os 14 Hz vistos uma vez no Raio-X), anota quando.

Quando acabares, diz-me o número e o que viste em cada ❌. Também posso ler os logs pelo ADB.
