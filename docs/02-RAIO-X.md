# Raio-X — instalar e testar

O Raio-X é o APK de diagnóstico. Mostra tudo o que o comando envia e testa cada saída.
No fim gera um relatório ✅/❌. Na app final, fica dentro das Definições.

## 1. Ativar o ADB por Wi-Fi (uma vez)

O comando ocupa a única porta USB do telemóvel, por isso o ADB é por Wi-Fi.

1. Abre **Definições → Acerca do telemóvel → Informações de software** e toca 7 vezes em **Número de compilação**. Mete o PIN. Aparece "Modo de programador ativado".
2. Em **Definições → Opções de programador**, liga a **Depuração sem fios**. O telemóvel e o PC têm de estar na mesma rede Wi-Fi. Aceita "Permitir nesta rede".
3. Toca no **texto** "Depuração sem fios" (não no interruptor) e escolhe **Emparelhar dispositivo com código de emparelhamento**. Aparecem um IP:porta e um código de 6 dígitos.
4. No PC, numa janela PowerShell, emparelha com esse IP:porta e escreve o código:
   ```
   E:\Android\Sdk\platform-tools\adb.exe pair 192.168.x.x:PORTA_DO_EMPARELHAMENTO
   ```

O emparelhamento só se faz uma vez.

## 2. Instalar o APK

Na pasta do projeto, em PowerShell:
```
.\tools\install-apk.ps1 192.168.x.x:PORTA
```
A porta é a que aparece no ecrã principal da **Depuração sem fios**. É **diferente** da porta do emparelhamento e muda sempre que o Wi-Fi ou a depuração se desligam.

**Sem PC:** copia `apk\OrimbuxPad-debug.apk` para o telemóvel (Quick Share, Google Drive…) e abre-o. Permite "Instalar apps desconhecidas" à app que abrir o ficheiro.

## 3. Primeiro arranque

1. Abre a **Orimbux Pad** e permite o **microfone** (é para o teste do micro do comando).
2. Liga o comando com o cabo USB-C. O Android pergunta "Abrir Orimbux Pad quando Wireless Controller estiver ligado?". Marca **Abrir sempre** e carrega **OK**.
   - A partir daí a permissão fica memorizada.
3. No topo deve aparecer **Comando ligado · xx %**.

## 4. Testes

### Separador "Entradas" (automático)
- Carrega em cada botão, incluindo PS, Create, Options, Mute e o clique do touchpad.
- Leva cada stick aos 4 extremos.
- Carrega o L2 e o R2 a meio e até ao fundo.
- Toca no touchpad com 1 dedo e depois com 2 ao mesmo tempo.
- Roda o comando (giroscópio). Pousa-o quieto uns segundos (acelerómetro).
- **Religação:** tira o cabo e volta a pô-lo.

Cada linha fica verde (✅) sozinha quando é detetada.

### Separador "Saídas" (confirmas tu)
- Carrega em **Testar** / **Ativar** em cada linha e confirma com ✅ (funcionou) ou ❌ (não funcionou).
- Nos gatilhos: ativa o modo e carrega no gatilho para o sentir.
- "R2: clique ritmado" simula o BEAT ROLL: um clique a cada 1/16 a 172 BPM.
- O **off** serve para confirmar que o gatilho volta a ficar livre.

### Separador "Áudio e latência"
**Testes rápidos**
- **Háptica (canais 3–4):** carrega em "Tocar háptica". Deves sentir uma vibração "musical" de 0,4 s nas mãos.
  - Se não sentires, escolhe outro **método** em cima (Oboe Quad, AudioTrack) e repete.
- **Jack:** liga auscultadores ao comando e carrega em "Tocar no jack".
- **Altifalante do comando:** só como diagnóstico (a app final não o usa).
- **Microfone:** carrega em "Gravar 3 s e ouvir", fala para o comando e ouve a gravação no telemóvel.

**Cenários de latência** — faz cada um e carrega em **Guardar resultado**:

| Cenário | Como ligar | O que testa |
|---|---|---|
| A | Comando direto ao telemóvel | Música nos altifalantes do telemóvel + háptica no comando, ao mesmo tempo |
| B | Comando direto, **auscultadores no jack do comando** | Música no jack (canais 1–2) + háptica (3–4) num só stream |
| A + hub | Telemóvel → hub USB-C PD → comando; carregador ligado ao hub | O mesmo que A, a carregar o telemóvel |
| B + hub | Igual, com auscultadores no comando | O mesmo que B, a carregar o telemóvel |

Sem hub, os dois cenários com hub aparecem no relatório como **➖ não testado**. Não contam como falha. Se um dia tiveres um hub, basta fazê-los e guardar o resultado.

Para cada cenário:
1. Escolhe A ou B. Para os testes com hub, liga o interruptor **Ligado por hub USB-C PD**. O ecrã mostra se o telemóvel está mesmo a carregar.
2. Carrega em **Abrir cenário**.
3. Liga a **Música de teste**: deves ouvir um beat a 172 BPM e sentir o kick no comando.
4. Carrega ✕ ○ □ △ ou as setas **pelo menos 10 vezes**, espaçadas. Cada toque faz um clique e um impulso háptico, e o ecrã mostra a latência (mínima, média e máxima).
5. Responde a "Ouvi o clique" e "Senti a háptica" e carrega em **Guardar resultado**.

**Como se mede a latência:**
- Conta do momento em que o SDL recebe o botão até o som sair no DAC, segundo o timestamp do próprio sistema de áudio.
- A isso somam-se 0–4 ms do USB do comando (média ~2 ms), que o telemóvel não consegue medir.
- "estimadas sem timestamp" quer dizer que o sistema não deu timestamp. Nesse caso o valor é só uma estimativa pelo tamanho do buffer.

### Separador "Relatório"
- Carrega em **Copiar** e cola o texto no chat.
- O relatório também fica guardado no telemóvel. Para o puxar para o PC:
  ```
  E:\Android\Sdk\platform-tools\adb.exe pull /sdcard/Android/data/pt.orimbux.pad/files/raio-x-relatorio.txt
  ```
- Os resultados ficam guardados mesmo que feches a app (por exemplo, para ligar o hub). **Apagar resultados** limpa tudo.

## 5. Se algo correr mal
- **"Comando desligado" com o cabo ligado:** tira e volta a pôr o cabo; confirma que aceitaste o diálogo USB.
- **"SDL falhou":** envia-me o relatório e os logs:
  ```
  E:\Android\Sdk\platform-tools\adb.exe logcat -d -s OrimbuxCtrl OrimbuxAudio OrimbuxSdl OrimbuxPull OrimbuxRaioX SDL hidapi > raio-x-log.txt
  ```

O que falhar no Raio-X é removido da app final, e eu digo-te o que muda.
