# code.py - ILI9341 + XPT2046 + MIDI KEY + 5 encoders
# Telas agrupadas por função, filtros nas primeiras telas
# Botões com label em 1 ou 2 linhas

import time
import board
import busio
import digitalio
import displayio
import terminalio
import rotaryio

from adafruit_display_text import label
from adafruit_ili9341 import ILI9341
from adafruit_display_shapes.roundrect import RoundRect

import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from adafruit_midi.control_change import ControlChange

# ============================================================
# TOUCH DRIVER XPT2046 (com SPI lock)
# ============================================================

class Touch:
    GET_X = 0b11010000
    GET_Y = 0b10010000

    def __init__(self, spi, cs, width=320, height=240,
                 x_min=100, x_max=1962, y_min=100, y_max=1900):
        self.spi = spi
        self.cs = cs
        self.cs.direction = digitalio.Direction.OUTPUT
        self.cs.value = True

        self.rx_buf = bytearray(3)
        self.tx_buf = bytearray(3)

        self.width = width
        self.height = height

        self.x_min = x_min
        self.x_max = x_max
        self.y_min = y_min
        self.y_max = y_max

        self.x_multiplier = width / float(x_max - x_min)
        self.x_add = x_min * -self.x_multiplier
        self.y_multiplier = height / float(y_max - y_min)
        self.y_add = y_min * -self.y_multiplier

    def _send_command(self, command):
        self.tx_buf[0] = command
        self.tx_buf[1] = 0
        self.tx_buf[2] = 0

        while not self.spi.try_lock():
            pass
        try:
            self.spi.configure(baudrate=1_000_000, phase=0, polarity=0)
            self.cs.value = False
            self.spi.write_readinto(self.tx_buf, self.rx_buf)
            self.cs.value = True
        finally:
            self.spi.unlock()

        return (self.rx_buf[1] << 4) | (self.rx_buf[2] >> 4)

    def _raw_touch(self):
        x = self._send_command(self.GET_X)
        y = self._send_command(self.GET_Y)
        if self.x_min <= x <= self.x_max and self.y_min <= y <= self.y_max:
            return (x, y)
        return None

    def _normalize(self, x, y):
        return (
            int(self.x_multiplier * x + self.x_add),
            int(self.y_multiplier * y + self.y_add),
        )

    def read(self):
        raw = self._raw_touch()
        if raw is None:
            return None
        return self._normalize(*raw)

# ============================================================
# DISPLAY / SPI
# ============================================================

DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240

displayio.release_displays()
spi = busio.SPI(clock=board.GP10, MOSI=board.GP11, MISO=board.GP12)

display_bus = displayio.FourWire(
    spi,
    command=board.GP14,
    chip_select=board.GP13,
    reset=board.GP15,
    baudrate=12000000,
)

display = ILI9341(display_bus, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, rotation=0)

# TOUCH
touch = Touch(spi, digitalio.DigitalInOut(board.GP9),
              width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)

# LED
led = digitalio.DigitalInOut(board.LED)
led.direction = digitalio.Direction.OUTPUT

# MIDI
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=3)

# ============================================================
# ENCODERS (5 unidades)
# ============================================================

encoder1 = rotaryio.IncrementalEncoder(board.GP0, board.GP1)
encoder2 = rotaryio.IncrementalEncoder(board.GP2, board.GP3)
encoder3 = rotaryio.IncrementalEncoder(board.GP4, board.GP5)
encoder4 = rotaryio.IncrementalEncoder(board.GP6, board.GP7)
encoder5 = rotaryio.IncrementalEncoder(board.GP16, board.GP17)

last_position1 = 0
last_position2 = 0
last_position3 = 0
last_position4 = 0
last_position5 = 0

reverse1 = -1
reverse2 = -1
reverse3 = -1
reverse4 = -1
reverse5 = -1

# ============================================================
# PÁGINAS (10 telas) – FILTROS nas primeiras telas
# campo "cc" é o número da NOTA MIDI
# ============================================================

PAGES = [
    # 0 - Menus / Função extra
    {
        "title": "0 - MENUS / EXTRA",
        "buttons": [
            {"label": "Main Menu",   "cc": 111},
            {"label": "Memory Menu", "cc": 112},
            {"label": "Mode Menu",   "cc": 113},
            {"label": "Noise Menu",  "cc": 114},
            {"label": "Band Menu",   "cc": 115},
            {"label": "TX Menu",     "cc": 116},
        ],
    },

    # 1 - Filtros básicos / AGC
    {
        "title": "1 - FILTROS 1",
        "buttons": [
            {"label": "AGC",               "cc": 21},
            {"label": "AGC Menu",          "cc": 22},
            {"label": "NB",                "cc": 23},
            {"label": "NR",                "cc": 24},
            {"label": "ANF",               "cc": 25},
            {"label": "CW Peak Fltr",      "cc": 26},
        ],
    },

    # 2 - Filtros avançados
    {
        "title": "2 - FILTROS 2",
        "buttons": [
            {"label": "Filter -",           "cc": 31},
            {"label": "Filter +",           "cc": 32},
            {"label": "Filter Cut",         "cc": 33},
            {"label": "Filter Cut Def",     "cc": 34},
            {"label": "RX Filter Menu",     "cc": 35},
            {"label": "Binaural",           "cc": 36},
        ],
    },

    # 3 - Bandas HF principais
    {
        "title": "3 - BANDAS HF 1",
        "buttons": [
            {"label": "6m",    "cc": 41},
            {"label": "10m",   "cc": 42},
            {"label": "12m",   "cc": 43},
            {"label": "15m",   "cc": 44},
            {"label": "17m",   "cc": 45},
            {"label": "20m",   "cc": 46},
        ],
    },

    # 4 - Bandas HF 2
    {
        "title": "4 - BANDAS HF 2",
        "buttons": [
            {"label": "30m",   "cc": 47},
            {"label": "40m",   "cc": 48},
            {"label": "60m",   "cc": 49},
            {"label": "70m",   "cc": 50},
            {"label": "80m",   "cc": 51},
            {"label": "160m",  "cc": 52},
        ],
    },

    # 5 - VFO / TUNE
    {
        "title": "5 - VFO / TUNE",
        "buttons": [
            {"label": "VFO Menu",   "cc": 81},
            {"label": "VFO Step -", "cc": 82},
            {"label": "VFO Step +", "cc": 83},
            {"label": "Tune",       "cc": 84},
            {"label": "Tune Full",  "cc": 85},
            {"label": "Tune Mem",   "cc": 86},
        ],
    },

    # 6 - RIT / XIT
    {
        "title": "6 - RIT / XIT",
        "buttons": [
            {"label": "RIT Clear",    "cc": 71},
            {"label": "RIT On/Off",   "cc": 72},
            {"label": "RIT -",        "cc": 73},
            {"label": "RIT +",        "cc": 74},
            {"label": "XIT On/Off",   "cc": 75},
            {"label": "RIT/XIT Cycle","cc": 76},
        ],
    },

    # 7 - RX / Diversity
    {
        "title": "7 - RX / DIV",
        "buttons": [
            {"label": "RX Menu",    "cc": 91},
            {"label": "RX1",        "cc": 92},
            {"label": "RX2",        "cc": 93},
            {"label": "DIV On/Off", "cc": 94},
            {"label": "DIV Menu",   "cc": 95},
            {"label": "Swap RX",    "cc": 96},
        ],
    },

    # 8 - TX / Áudio / Compressor
    {
        "title": "8 - TX / AUDIO",
        "buttons": [
            {"label": "MOX",            "cc": 101},
            {"label": "Cmpr On/Off",    "cc": 102},
            {"label": "VOX On/Off",     "cc": 103},
            {"label": "CTUN",           "cc": 104},
            {"label": "Mute",           "cc": 105},
            {"label": "Mute RX1",       "cc": 106},
        ],
    },
]

NUM_PAGES = len(PAGES)

# ============================================================
# CALIBRAÇÃO FIXA (8 pontos)
# (ajuste depois conforme o layout real)
# ============================================================

calibration_points = [
    (217,186), # BTN0
    (219,132), # BTN1
    (195,89),  # BTN2
    (69,186),  # BTN3
    (83,128),  # BTN4
    (96,70),   # BTN5
    (144,222), # <
    (155,28),  # >
]

def nearest_button_index(x, y):
    best = None
    best_d2 = None
    for i,(cx,cy) in enumerate(calibration_points):
        dx=x-cx; dy=y-cy
        d2=dx*dx+dy*dy
        if best_d2 is None or d2<best_d2:
            best=i; best_d2=d2
    return best

# ============================================================
# BOTÃO
# ============================================================

BTN_COLORS = [0xFF5555,0x55FF55,0x5555FF,0xFFFF55,0xFF55FF,0x55FFFF]
BTN_COLORS_DOWN = [0xAA3030,0x30AA30,0x3030AA,0xAAAA30,0xAA30AA,0x30AAAA]

class TouchButton:
    def __init__(self,x,y,w,h,text,midi_cfg,color,color_down,kind,group,text_color=0x000000):
        self.x=x; self.y=y; self.w=w; self.h=h
        self.midi_cfg=midi_cfg
        self.kind=kind
        self.pressed=False
        r=min(w,h)//6

        self.rect=RoundRect(x,y,w,h,r=r,fill=color,outline=0x202020,stroke=2)
        group.append(self.rect)

        # ----- label em 1 ou 2 linhas -----
        parts = text.split(" ")

        labels = []

        if len(parts) == 1:
            line1 = text
            lbl = label.Label(terminalio.FONT, text=line1, color=text_color, scale=1)
            lbl.anchor_point = (0.5, 0.5)
            lbl.anchored_position = (x + w//2, y + h//2)
            group.append(lbl)
            labels.append(lbl)

        elif len(parts) == 2:
            line1 = parts[0]
            line2 = parts[1]

            lbl1 = label.Label(terminalio.FONT, text=line1, color=0x000000, scale=1)
            lbl2 = label.Label(terminalio.FONT, text=line2, color=0x000000, scale=1)

            lbl1.anchor_point = (0.5, 0.5)
            lbl2.anchor_point = (0.5, 0.5)

            lbl1.anchored_position = (x + w//2, y + h//2 - 8)
            lbl2.anchored_position = (x + w//2, y + h//2 + 8)

            group.append(lbl1)
            group.append(lbl2)
            labels.extend([lbl1, lbl2])

        elif len(parts) == 3:
            line1 = parts[0]
            line2 = " ".join(parts[1:])

            lbl1 = label.Label(terminalio.FONT, text=line1, color=0x000000, scale=1)
            lbl2 = label.Label(terminalio.FONT, text=line2, color=0x000000, scale=1)

            lbl1.anchor_point = (0.5, 0.5)
            lbl2.anchor_point = (0.5, 0.5)

            lbl1.anchored_position = (x + w//2, y + h//2 - 8)
            lbl2.anchored_position = (x + w//2, y + h//2 + 8)

            group.append(lbl1)
            group.append(lbl2)
            labels.extend([lbl1, lbl2])

        else:
            # 4+ palavras → divide meio a meio
            mid = len(parts) // 2
            line1 = " ".join(parts[:mid])
            line2 = " ".join(parts[mid:])

            lbl1 = label.Label(terminalio.FONT, text=line1, color=0x000000, scale=1)
            lbl2 = label.Label(terminalio.FONT, text=line2, color=0x000000, scale=1)

            lbl1.anchor_point = (0.5, 0.5)
            lbl2.anchor_point = (0.5, 0.5)

            lbl1.anchored_position = (x + w//2, y + h//2 - 8)
            lbl2.anchored_position = (x + w//2, y + h//2 + 8)

            group.append(lbl1)
            group.append(lbl2)
            labels.extend([lbl1, lbl2])

        self.labels = labels
        self.color=color
        self.color_down=color_down

    def set_visual(self,down):
        self.rect.fill=self.color_down if down else self.color

def create_page_ui(idx_page):
    group = displayio.Group()
    midi_btn=[]
    nav_btn=[]

    cfg = PAGES[idx_page]["buttons"]

    # ----- título -----
    title_label = label.Label(
        terminalio.FONT,
        text=PAGES[idx_page]["title"],
        color=0xFFFFFF,
        scale=2
    )
    title_label.anchor_point = (0.5, 0)
    title_label.anchored_position = (DISPLAY_WIDTH // 2, 2)
    group.append(title_label)

    title_h = 28  # altura reservada para o título

    nav_w=40
    gap=6
    cols=3
    rows=2

    usable = DISPLAY_WIDTH - nav_w*2
    bw = (usable - (cols+1)*gap) // cols
    bh = (DISPLAY_HEIGHT - title_h - (rows+1)*gap) // rows

    k=0
    for r in range(rows):
        for c in range(cols):
            x = nav_w + gap + c*(bw+gap)
            y = title_h + gap + r*(bh+gap)
            cfgb = cfg[k]
            btn = TouchButton(
                x,y,bw,bh,cfgb["label"],cfgb,
                BTN_COLORS[k],BTN_COLORS_DOWN[k],
                "midi",group
            )
            midi_btn.append(btn)
            k+=1

    # <
    btn_prev = TouchButton(
        4, title_h + gap,
        nav_w-8, DISPLAY_HEIGHT - title_h - 2*gap,
        "<", None,
        0x404040, 0x808080, "nav_prev", group,
        text_color=0xFFFFFF   # <-- NOVO
    )

    # >
    btn_next = TouchButton(
        DISPLAY_WIDTH-nav_w+4, title_h + gap,
        nav_w-8, DISPLAY_HEIGHT - title_h - 2*gap,
        ">", None,
        0x404040, 0x808080, "nav_next", group,
        text_color=0xFFFFFF   # <-- NOVO
    )

    return group,midi_btn,nav_btn

# ============================================================
# MIDI NOTE (KEY) – botões da tela
# ============================================================

def midi_on(note):
    midi.send(NoteOn(note, 60))

def midi_off(note):
    midi.send(NoteOff(note, 0))

# ============================================================
# LOOP
# ============================================================

page=0
ui,midi_btn,nav_btn = create_page_ui(page)
display.show(ui)

active=None
touch_down=False

print("MIDI Touch Controller + Encoders – filtros primeiro")

while True:
    # ----- TOUCH -----
    p = touch.read()

    if p is not None:
        x,y = p
        led.value = True

        if not touch_down:
            touch_down = True
            idx = nearest_button_index(x,y)

            if idx < 6:
                active = idx
                b = midi_btn[idx]
                b.set_visual(True)
                midi_on(b.midi_cfg["cc"])

            elif idx == 6:
                page = (page - 1) % NUM_PAGES
                ui,midi_btn,nav_btn = create_page_ui(page)
                display.show(ui)

            elif idx == 7:
                page = (page + 1) % NUM_PAGES
                ui,midi_btn,nav_btn = create_page_ui(page)
                display.show(ui)

    else:
        led.value = False

        if touch_down:
            if active is not None:
                b = midi_btn[active]
                midi_off(b.midi_cfg["cc"])
                b.set_visual(False)
                active = None
            touch_down = False

    # ----- ENCODERS (ControlChange como no código original) -----

    pos1 = encoder1.position
    if pos1 != last_position1:
        if pos1 < last_position1:
            midi.send(ControlChange(11, 64 + reverse1))
        elif pos1 > last_position1:
            midi.send(ControlChange(11, 64 - reverse1))
        last_position1 = pos1

    pos2 = encoder2.position
    if pos2 != last_position2:
        if pos2 < last_position2:
            midi.send(ControlChange(12, 64 + reverse2))
        elif pos2 > last_position2:
            midi.send(ControlChange(12, 64 - reverse2))
        last_position2 = pos2

    pos3 = encoder3.position
    if pos3 != last_position3:
        if pos3 < last_position3:
            midi.send(ControlChange(13, 64 + reverse3))
        elif pos3 > last_position3:
            midi.send(ControlChange(13, 64 - reverse3))
        last_position3 = pos3

    pos4 = encoder4.position
    if pos4 != last_position4:
        if pos4 < last_position4:
            midi.send(ControlChange(14, 64 + reverse4))
        elif pos4 > last_position4:
            midi.send(ControlChange(14, 64 - reverse4))
        last_position4 = pos4

    pos5 = encoder5.position
    if pos5 != last_position5:
        if pos5 < last_position5:
            midi.send(ControlChange(15, 64 + reverse5))
        elif pos5 > last_position5:
            midi.send(ControlChange(15, 64 - reverse5))
        last_position5 = pos5

    time.sleep(0.02)
