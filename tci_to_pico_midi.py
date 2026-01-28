#!/usr/bin/env python3
import asyncio
import os
import re
import struct
import sys
import time

import mido
import websockets

# =========================
# Config
# =========================
TCI_URL = os.environ.get("TCI_URL", "ws://127.0.0.1:50001")
MIDI_OUT_NAME_HINT = os.environ.get("MIDI_OUT_HINT", "Pico")
SEND_INTERVAL_MS = int(os.environ.get("SEND_INTERVAL_MS", "150"))  # rate limit
DEBUG = os.environ.get("DEBUG", "0") == "1"

# SysEx format (custom)
# F0 7D 01 <vfo_id:1> <freq_hz_u64_le:8> F7
# 0x7D = non-commercial manufacturer ID
SYSEX_MFR = 0x7D
SYSEX_CMD_FREQ = 0x01

# Heuristics to find frequency in TCI messages
# Common examples in TCI ecosystem: "VFO:0,14074000;" etc.
RE_VFO = re.compile(r"(?i)\bVFO\s*:\s*(\d+)\s*,\s*(\d+)\s*;")
RE_ANY_HZ = re.compile(r"(\d{4,})")  # captures integers >= 4 digits

def pick_midi_out():
    outs = mido.get_output_names()
    if not outs:
        raise RuntimeError("No MIDI output found. Check if the Pico is enumerated as USB-MIDI.")
    # try by name
    for name in outs:
        if MIDI_OUT_NAME_HINT.lower() in name.lower():
            return name
    # fallback: first
    return outs[0]

def build_sysex_freq(vfo_id: int, freq_hz: int):
    if freq_hz < 0:
        freq_hz = 0
    if freq_hz > 0xFFFFFFFFFFFFFFFF:
        freq_hz = 0xFFFFFFFFFFFFFFFF
    payload = bytearray()
    payload.append(SYSEX_MFR)
    payload.append(SYSEX_CMD_FREQ)
    payload.append(vfo_id & 0xFF)
    payload.extend(struct.pack("<Q", freq_hz))
    return mido.Message("sysex", data=list(payload))

def parse_freq_from_tci(msg: str):
    # 1) VFO:<id>,<hz>;
    m = RE_VFO.search(msg)
    if m:
        vfo_id = int(m.group(1))
        hz = int(m.group(2))
        return vfo_id, hz

    # 2) fallback: try to find a number that looks like frequency in Hz
    # (e.g.: 7074000, 145500000 etc). Takes the largest plausible number.
    nums = [int(x) for x in RE_ANY_HZ.findall(msg)]
    if not nums:
        return None
    # choose plausible candidate (1 kHz .. 10 GHz)
    candidates = [n for n in nums if 1_000 <= n <= 10_000_000_000]
    if not candidates:
        return None
    hz = max(candidates)
    return 0, hz  # vfo_id unknown

async def main():
    midi_name = pick_midi_out()
    if DEBUG:
        print("TCI_URL =", TCI_URL)
        print("MIDI out =", midi_name)

    last_send = 0.0
    last_hz = None
    last_vfo = None

    with mido.open_output(midi_name) as midi_out:
        async with websockets.connect(TCI_URL, ping_interval=20, ping_timeout=20) as ws:
            # Some TCI servers send initial state automatically when connecting.
            while True:
                raw = await ws.recv()
                if not isinstance(raw, str):
                    # ignore binary (audio/IQ etc)
                    continue

                if DEBUG:
                    print("TCI:", raw.strip())

                parsed = parse_freq_from_tci(raw)
                if not parsed:
                    continue
                vfo_id, hz = parsed

                # de-dup + rate limit
                now = time.time()
                if (hz == last_hz) and (vfo_id == last_vfo):
                    continue
                if (now - last_send) * 1000.0 < SEND_INTERVAL_MS:
                    continue

                syx = build_sysex_freq(vfo_id, hz)
                midi_out.send(syx)

                last_send = now
                last_hz = hz
                last_vfo = vfo_id

                if DEBUG:
                    print(f"-> MIDI SysEx freq: vfo={vfo_id} hz={hz}")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
