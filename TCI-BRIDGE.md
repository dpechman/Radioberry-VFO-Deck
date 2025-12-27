TCI = Transceiver Control Interface.

É uma API de controle via rede (TCP/WebSocket) usada por SDRs para trocar estado em tempo real: frequência, VFO, modo, PTT, filtros, etc.
No DeskHPSDR, a TCI envia eventos automaticamente quando algo muda (ex.: girou o VFO → manda nova frequência).

TCI (WebSocket) → bridge no Raspberry Pi (Radioberry) → SysEx pela USB-MIDI da Pico.

TCI é WebSocket full-duplex e o servidor manda mudanças de estado pros clientes (sem polling). 
GitHub
+1

Ativvação:

Ative o TCI no deskHPSDR (nas opções/compile flags dele; depende do build).

Rode um bridge Python no Raspberry:

Conecta no TCI (ex: ws://127.0.0.1:50001)

Quando detectar frequência/VFO, envia um SysEx para a Pico via USB-MIDI.

Bridge pronto (Python) — copia e cola

Instala dependências:

python3 -m pip install --user websockets mido python-rtmidi

Script tci_to_pico_midi.py

Rodar:

DEBUG=1 TCI_URL=ws://127.0.0.1:50001 MIDI_OUT_HINT=Pico python3 tci_to_pico_midi.py

No firmware da Pico (TinyUSB MIDI/Arduino/SDK), ler SysEx:

0x7D 0x01 vfo_id freq_u64_le

Como acessar a TCI no DeskHPSDR
1) Ativar a TCI

Depende do build, mas em geral é uma destas formas:

A. Pelo menu/configuração

Procura por TCI ou Enable TCI nas opções do DeskHPSDR.

Porta padrão costuma ser 50001.

B. Na compilação

make.config.deskhpsdr:

TCI=ON


Recompila.

Quando ativa, o DeskHPSDR sobe um servidor TCI local.

2) Conectar na TCI (teste rápido)

A TCI usa WebSocket.

Teste com websocat:

websocat ws://127.0.0.1:50001


Ao conectar, você já deve ver mensagens tipo:

VFO:0,7074000;
MODE:LSB;


Gire o VFO no DeskHPSDR → a frequência muda em tempo real.

3) O que você recebe pela TCI

Mensagens de texto simples, por exemplo:

VFO:0,14074000; → VFO A em Hz

VFO:1,7050000; → VFO B

MODE:USB;

PTT:1; / PTT:0;

Não precisa polling. É push.
