TCI = Transceiver Control Interface.

It's a network control API (TCP/WebSocket) used by SDRs to exchange status in real time: frequency, VFO, mode, PTT, filters, etc.
In DeskHPSDR, TCI automatically sends events when something changes (e.g.: turned VFO → sends new frequency).

TCI (WebSocket) → bridge on Raspberry Pi (Radioberry) → SysEx via Pico's USB-MIDI.

TCI is full-duplex WebSocket and the server sends status changes to clients (no polling). 
GitHub
+1

Activation:

Enable TCI in deskHPSDR (in its options/compile flags; depends on build).

Run a Python bridge on the Raspberry:

Connects to TCI (e.g.: ws://127.0.0.1:50001)

When it detects frequency/VFO, sends a SysEx to the Pico via USB-MIDI.

Ready bridge (Python) — copy and paste

Install dependencies:

python3 -m pip install --user websockets mido python-rtmidi

Script tci_to_pico_midi.py

Run:

DEBUG=1 TCI_URL=ws://127.0.0.1:50001 MIDI_OUT_HINT=Pico python3 tci_to_pico_midi.py

On the Pico firmware (TinyUSB MIDI/Arduino/SDK), read SysEx:

0x7D 0x01 vfo_id freq_u64_le

How to access TCI in DeskHPSDR
1) Enable TCI

Depends on the build, but generally it's one of these ways:

A. Through menu/configuration

Look for TCI or Enable TCI in DeskHPSDR options.

Default port is usually 50001.

B. At compilation

make.config.deskhpsdr:

TCI=ON


Recompile.

When active, DeskHPSDR starts a local TCI server.

2) Connect to TCI (quick test)

TCI uses WebSocket.

Test with websocat:

websocat ws://127.0.0.1:50001


When connecting, you should already see messages like:

VFO:0,7074000;
MODE:LSB;


Turn the VFO in DeskHPSDR → the frequency changes in real time.

3) What you receive through TCI

Simple text messages, for example:

VFO:0,14074000; → VFO A in Hz

VFO:1,7050000; → VFO B

MODE:USB;

PTT:1; / PTT:0;

No polling needed. It's push.
