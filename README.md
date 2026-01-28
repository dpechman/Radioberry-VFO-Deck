# Radioberry VFO Deck

> **⚠️ Hardware Platform Change**: This project has been migrated from Raspberry Pi Pico to **ESP32 (JC4827W543 board)**. The original Pico-based version is preserved in the [`pico-version`](https://github.com/dpechman/Radioberry-VFO-Deck/tree/pico-version) branch.

The **Radioberry VFO Deck** is a dedicated auxiliary display and control interface for software-defined radio (SDR) applications, specifically designed to work with **piHPSDR**, **DeskHPSDR**, and other compatible SDR software.

<p align="center">
  <img src="vfodeck.jpg" width="1024">
</p>

Built around the **ESP32-S3** microcontroller on the **JC4827W543** board (480x272 capacitive touch display), this project provides a professional, hands-on control experience with both touch interface and physical encoders.

---

## Key Features

- **Integrated 480x272 Capacitive Touch Display** (JC4827W543 board)
- **Dual Communication Protocols**: USB MIDI and CAT (Computer Aided Transceiver)
- **Physical Encoder Support** for VFO tuning
- **Auxiliary Physical Buttons** for direct control
- **Multi-Page Touch Interface** with 11+ screens organized by function
- **Real-Time Frequency Display** with live updates via CAT protocol
- **Mode Selection** (LSB, USB, CW, FM, AM, FSK, CWR, FSKR)
- **Independent Operation** alongside main SDR graphical interface

---

## Hardware Platform

### Main Board: JC4827W543
- **Microcontroller**: ESP32-S3 (dual-core, 240 MHz)
- **Display**: 4.3" 480x272 RGB TFT with capacitive touch (GT911)
- **Flash**: 16MB
- **PSRAM**: 8MB
- **Connectivity**: USB-C (native USB for MIDI + CAT serial)
- **Interface**: Arduino-compatible

### External Components
- **VFO Encoder**: Rotary encoder with quadrature outputs
- **Auxiliary Buttons**: Configurable physical buttons for step control, mode switching, etc.
- **(Optional) Additional Encoders**: For filter, AF/RF gain, etc.

### Schematic & PCB Files
All KiCAD design files are located in the [`HARDWARE/`](HARDWARE/) directory.

---

## Communication Protocols

### USB MIDI (OUT)
The controller sends MIDI messages to control SDR software:
- **Control Change (CC)**: Encoders send CC15 (wheel) for VFO tuning
- **Note On/Off**: Touch buttons send MIDI notes for functions (filters, bands, menus, etc.)
- **Channel**: MIDI Channel 1 (configurable)

### CAT Protocol (Bidirectional Serial)
Compatible with Kenwood/Yaesu CAT command sets:
- **Frequency Updates**: Receives `FA;` and `FB;` commands for VFO A/B
- **Mode Updates**: Receives `MD;` commands (LSB, USB, CW, FM, etc.)
- **Status Queries**: Periodic polling for real-time display sync
- **Direct Commands**: Can send mode changes directly to radio

The CAT interface runs over USB CDC (second serial port), enabling simultaneous MIDI control and frequency/mode display without external bridges.

---

## Software Architecture

### ESP32 Firmware
- **Platform**: Arduino framework (ESP32-S3)
- **Libraries**:
  - `Arduino_GFX_Library` for display
  - `bb_captouch` for GT911 touch controller
  - `USB.h`, `USBMIDI.h`, `USBCDC.h` for dual USB functionality
- **Source Code**: [`CODE/RADIOBERRY_MIDI_CONTROLLER.ino`](CODE/RADIOBERRY_MIDI_CONTROLLER.ino)

### Host Computer Integration
```
SDR Software (piHPSDR/DeskHPSDR)
    ↕ CAT Protocol (Serial/USB)
VFO Deck ESP32
    ↕ MIDI (USB)
SDR Software MIDI Input
```

The VFO Deck connects via a single USB cable providing:
1. **Power**
2. **MIDI control output** (button presses, encoder turns)
3. **CAT serial communication** (frequency/mode display)

---

## Touch Interface Pages

### Page 0: VFO Screen (Main)
- **Large Frequency Display**: Shows active VFO frequency with dot separators
- **VFO Selection**: Displays A/B selection
- **Mode Display**: Current operating mode (LSB, USB, CW, etc.)
- **Step Display**: Current tuning step (1 Hz to 100 kHz)
- **Four Bottom Buttons**:
  - `STEP -` / `STEP +`: Change tuning step
  - `A/B`: Toggle between VFO A and B
  - `MODE`: Cycle through modes or open mode page

### Page 1: Modes
Direct mode selection via CAT commands:
- LSB, USB, CW, FM, AM, FSK, CWR, FSKR

### Page 2: Menus
Access to common menu functions:
- Main Menu, Memory Menu, Mode Menu, Noise Menu
- Band Menu, TX Menu, RX Menu, Display Menu

### Page 3: Filters
Primary filter controls:
- AGC, AGC Menu, NB (Noise Blanker), NR (Noise Reduction)
- ANF (Auto Notch Filter), CW Peak Filter, APF, Squelch
- Filter Width controls (-, +, Cut, Default)

### Page 4: Filters 2
Advanced filter options:
- RX Filter Menu, Binaural, Notch, Notch Auto

### Pages 5-6: Bands
Quick band selection (6m to 160m, including 2m and 70cm)

### Page 7: VFO/Tune
- VFO Menu, Step controls, Tune functions
- CTUN (Continuous Tune), Lock

### Page 8: RIT/XIT
- RIT Clear, On/Off, +/-
- XIT On/Off, Split, A/B swap

### Page 9: RX/Diversity
- RX Menu, RX1/RX2 selection
- Diversity controls, RF/AF Gain

### Page 10: TX/Audio
- MOX, Compressor, VOX
- Mute controls, Power, Mic Gain

---

## Physical Controls

### VFO Encoder
- **Function**: Continuous frequency tuning
- **Hardware**: Quadrature rotary encoder (pins 17, 18)
- **MIDI Output**: CC15 (wheel) with values 64±1 for direction
- **Interrupt-Driven**: Ensures no missed steps

### Auxiliary Buttons
Configurable physical buttons can be mapped to:
- Step increment/decrement (MIDI notes 120, 121)
- Mode cycling
- PTT (Push-to-Talk)
- Band up/down
- Custom MIDI notes

---

## Project Status

- [x] Base firmware structure with ESP32-S3
- [x] Touch interface with 11 functional pages
- [x] VFO encoder integration (MIDI CC)
- [x] CAT protocol receive (frequency, mode)
- [x] Real-time frequency display with formatting
- [x] Multi-page navigation system
- [x] MIDI note output for all buttons
- [ ] CAT mode change commands (transmit)
- [ ] Additional encoder support (filter, gain)
- [ ] Hardware PCB design in KiCAD
- [ ] PTT physical button integration
- [ ] Full CAT command implementation
- [ ] User configuration system (MIDI channel, CAT baud, etc.)

---

## Development Setup

### Arduino IDE
1. Install **ESP32 board support**: 
   - Add to Board Manager URLs: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Install "esp32" by Espressif Systems (v3.0.0+)
2. Select board: **ESP32S3 Dev Module**
3. Configure USB Mode: **USB-OTG (TinyUSB)**
4. Install required libraries:
   - `Arduino_GFX` (by moononournation)
   - `bb_captouch` (by bitbank2)

### PlatformIO
See `platformio.ini` in the CODE directory (coming soon).

---

## MIDI Mapping Reference

### Control Changes
| CC Number | Function | Value Range |
|-----------|----------|-------------|
| 15        | VFO Encoder (Wheel) | 63=CW, 65=CCW |

### Note Numbers
Refer to the source code [`pages[]` array](CODE/RADIOBERRY_MIDI_CONTROLLER.ino) for complete button-to-note mapping. Examples:
- **21-28**: Filter controls
- **31-38**: Filter advanced
- **41-56**: Band selection
- **71-78**: RIT/XIT
- **81-88**: VFO/Tune
- **91-98**: RX/Diversity
- **101-108**: TX/Audio
- **111-118**: Menus
- **120-121**: Step buttons

---

## CAT Command Reference

### Frequency Commands
- `FA;` - Query VFO A frequency
- `FA00014074000;` - Set VFO A to 14.074 MHz
- `FB;` - Query VFO B frequency
- `FB00007074000;` - Set VFO B to 7.074 MHz

### Mode Commands
- `MD;` - Query current mode
- `MD1;` - Set LSB
- `MD2;` - Set USB
- `MD3;` - Set CW
- `MD4;` - Set FM
- `MD5;` - Set AM

---

## Building Your Own

1. **Get the Hardware**:
   - Order JC4827W543 board (search AliExpress, Amazon, or electronics suppliers)
   - Source rotary encoder (KY-040 or similar)
   - Optional: tactile buttons, enclosure

2. **Flash Firmware**:
   - Open `CODE/RADIOBERRY_MIDI_CONTROLLER.ino` in Arduino IDE
   - Configure board settings (ESP32S3, TinyUSB)
   - Upload to board

3. **Connect to SDR Software**:
   - Configure piHPSDR/DeskHPSDR for CAT control (serial port)
   - Enable MIDI input and map CC15/notes to functions
   - Adjust polling rate and MIDI channel if needed

4. **Customize**:
   - Modify button labels and MIDI note assignments
   - Add/remove pages in the `pages[]` array
   - Adjust display colors and layout constants

---

## Contributing

This project welcomes contributions! Areas of interest:
- Hardware design improvements (PCB layout, enclosure)
- Additional SDR software integration
- Enhanced CAT protocol support
- UI/UX improvements
- Documentation and tutorials

---

## License

This project is open source. See [LICENSE](LICENSE) for details.

---

## Links

- **Original Pico Version**: [`pico-version` branch](https://github.com/dpechman/Radioberry-VFO-Deck/tree/pico-version)
- **Hardware Files**: [HARDWARE/](HARDWARE/)
- **Source Code**: [CODE/](CODE/)

---

## Acknowledgments

- **piHPSDR** and **DeskHPSDR** teams for exceptional SDR software
- **Radioberry** project for inspiring hardware integration
- **Arduino community** for ESP32 ecosystem and libraries
- **bb_captouch** and **Arduino_GFX** library authors
