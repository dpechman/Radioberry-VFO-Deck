# Project Development Guidelines

## Language Requirements

**ALL code comments, documentation, commit messages, and technical communications MUST be written in ENGLISH.**

This includes:
- Code comments (inline and block)
- Function/variable/class documentation
- README files and technical documentation
- Git commit messages
- Issue descriptions and pull requests
- Code review comments
- Technical design documents

## Project Overview

The **Radioberry VFO Deck** is an ESP32-based auxiliary control interface for SDR (Software Defined Radio) applications. It provides both a touchscreen interface and physical controls (encoders and buttons) to enhance the user experience when operating piHPSDR, DeskHPSDR, and compatible SDR software.

## Hardware Platform

- **Board**: JC4827W543 (ESP32-S3 with integrated 480x272 capacitive touch display)
- **Microcontroller**: ESP32-S3 (dual-core, 240 MHz, 16MB Flash, 8MB PSRAM)
- **Display**: 4.3" 480x272 RGB TFT with GT911 capacitive touch controller
- **Connectivity**: USB-C providing power, MIDI, and CAT serial communication
- **External I/O**: Rotary encoder for VFO tuning, auxiliary buttons for additional functions

## Communication Protocols

### USB MIDI (Output)
- **Purpose**: Send control commands from physical and touch buttons to SDR software
- **Implementation**: TinyUSB MIDI device class
- **Protocol Details**:
  - Control Change (CC) messages for encoders (CC15 for VFO wheel)
  - Note On/Off messages for button presses
  - MIDI Channel 1 (configurable)
  - Values: 64±1 for relative encoder movement

### CAT Protocol (Bidirectional)
- **Purpose**: Real-time frequency and mode display, bidirectional control
- **Implementation**: USB CDC (serial) device
- **Compatible With**: Kenwood/Yaesu CAT command sets
- **Key Commands**:
  - `FA;` / `FB;` - VFO A/B frequency queries and updates
  - `MD;` - Mode query and setting (LSB, USB, CW, FM, AM, etc.)
  - Semicolon-terminated ASCII commands
- **Features**:
  - Periodic polling for real-time updates
  - No external bridge required (direct USB connection)
  - Display shows current frequency, VFO selection, mode, and step

## Software Architecture

### Firmware Structure
- **Platform**: Arduino framework for ESP32-S3
- **Main File**: `CODE/RADIOBERRY_MIDI_CONTROLLER.ino`
- **Key Libraries**:
  - `Arduino_GFX_Library` - Display rendering
  - `bb_captouch` - GT911 touch controller driver
  - `USB.h`, `USBMIDI.h`, `USBCDC.h` - Dual USB functionality

### Code Organization
```
CODE/
  └── RADIOBERRY_MIDI_CONTROLLER.ino  # Main firmware (monolithic for Arduino compatibility)

HARDWARE/
  └── (KiCAD schematic and PCB files)
```

### User Interface Design

#### Multi-Page Touch Interface
The system implements 11 functional pages accessible via left/right navigation buttons:

1. **Page 0 - VFO Screen (Main)**
   - Large frequency display (Hz with dot separators)
   - Active VFO indicator (A/B)
   - Current mode display (LSB, USB, CW, etc.)
   - Tuning step display
   - Four control buttons: STEP-, A/B, MODE, STEP+

2. **Page 1 - Modes**
   - Direct mode selection (8 modes)
   - Sends CAT commands for mode changes

3. **Pages 2-10 - Function Groups**
   - 4×3 button grid (12 buttons per page)
   - Organized by function: Filters, Bands, VFO/Tune, RIT/XIT, RX/DIV, TX/Audio, Menus
   - Each button sends unique MIDI note

#### Touch Handling
- **Capacitive Touch**: GT911 controller with interrupt-driven updates
- **Touch Lock Mechanism**: Prevents multiple actions from single touch
- **Debounce**: 250ms for navigation buttons
- **Button States**: Visual feedback (color change on press)
- **Note Mode**: Buttons send Note On while pressed, Note Off on release

#### Physical Controls
- **VFO Encoder**: Interrupt-driven quadrature encoder (pins 17, 18)
  - Sends MIDI CC15 with value 63 (CCW) or 65 (CW)
  - Used for continuous frequency tuning in SDR software
- **Auxiliary Buttons** (configurable):
  - Step control (MIDI notes 120, 121)
  - Mode cycling
  - PTT, band switching, etc.

### CAT Protocol Implementation

#### Receive Handler
- Parses semicolon-terminated commands
- Updates internal state (frequency, mode)
- Triggers display updates only when values change
- Timeout detection (1200ms) for "CAT Alive" status

#### Command Parsing
```
FA00014074000;  →  VFO A = 14.074 MHz
FB00007074000;  →  VFO B = 7.074 MHz
MD2;            →  Mode = USB
```

#### Polling Strategy
- Poll every 200ms for frequency/mode updates
- Reduces unnecessary traffic while maintaining responsiveness

## Development Guidelines

### Code Style
- **Comments**: Describe WHY, not just WHAT (e.g., "// prevent duplicate actions from single touch" rather than "// set lock to true")
- **Naming**: Use descriptive names (`vfoAHz` not `f1`, `touchLocked` not `tl`)
- **Constants**: Use `#define` for hardware pins and configuration values
- **Magic Numbers**: Define as named constants at top of file
- **Formatting**: Consistent indentation (2 spaces), K&R brace style

### Variable Naming Conventions
- `camelCase` for variables and functions
- `UPPER_CASE` for constants and defines
- Hungarian notation for unit clarity: `vfoAHz`, `lastCatRxMs`, `NAV_DEBOUNCE_MS`

### Function Organization
- Group related functions (drawing, CAT, MIDI, touch handling)
- Use section comment blocks: `/* ================= SECTION ================= */`
- Keep functions focused on single responsibility
- Extract magic numbers to constants

### Display Updates
- **Dirty Flag Pattern**: Only redraw when state changes
- **Partial Updates**: Update only changed regions when possible
- **Color Scheme**: Define colors as named constants (WHITE, BLACK, GREY, etc.)
- **Text Formatting**: Helper functions for frequency display (dot separators)

### MIDI Implementation
- **Channel**: Configurable (default: 1)
- **CC15**: Reserved for VFO encoder wheel
- **Notes 21-121**: Function buttons
- **Velocity**: 127 for Note On, 0 for Note Off
- **Timing**: Send immediately, no buffering

### Error Handling
- Check return values for critical operations
- Implement timeouts for CAT communication
- Visual indicators for error states (e.g., "CAT disconnected")

## Testing Procedures

### Hardware Testing
1. **Display**: Verify all pages render correctly
2. **Touch**: Test all button regions, edge cases, multi-touch rejection
3. **Encoder**: Verify both directions, no missed steps
4. **USB**: Confirm MIDI and CDC enumerate properly

### Integration Testing
1. **MIDI Output**: Monitor with MIDI monitor software
2. **CAT Communication**: Test with terminal emulator
3. **SDR Integration**: Full workflow with piHPSDR/DeskHPSDR
4. **Performance**: Ensure UI remains responsive during CAT updates

## Future Enhancements

### Planned Features
- [ ] Additional encoders (filter width, AF/RF gain)
- [ ] PTT physical button with logic-level output
- [ ] Configuration system (EEPROM/preferences)
- [ ] User-customizable button mappings
- [ ] S-meter display on VFO screen
- [ ] Spectrum waterfall integration
- [ ] WiFi control option (alternative to USB)

### Hardware Extensions
- [ ] Backplane PCB for encoder/button mounting
- [ ] 3D-printed enclosure design
- [ ] Expansion connector for custom I/O
- [ ] LED indicators for TX/RX status

## Contribution Workflow

1. **Branch Naming**: `feature/description`, `bugfix/description`, `hardware/description`
2. **Commit Messages**: Imperative mood, < 72 chars, English only
   - Good: "Add CAT mode change command handler"
   - Bad: "added stuff", "mudanças no CAT"
3. **Pull Requests**: Include testing notes and screenshots if UI-related
4. **Code Review**: Focus on functionality, readability, and maintainability

## Documentation Requirements

### Code Documentation
- **File Headers**: Purpose, author, key dependencies
- **Function Comments**: Purpose, parameters, return values, side effects
- **Complex Logic**: Explain algorithm or reasoning
- **Hardware Interfaces**: Pin assignments, protocol details

### External Documentation
- **README.md**: Project overview, setup instructions, user guide
- **Hardware Files**: Component values, assembly notes, BOM
- **API Reference**: MIDI note mapping, CAT command support
- **Tutorials**: Getting started, customization examples

## Build and Deployment

### Arduino IDE Setup
1. Install ESP32 board support (v3.0.0+)
2. Select "ESP32S3 Dev Module"
3. Configure: USB Mode = "USB-OTG (TinyUSB)"
4. Install libraries: Arduino_GFX, bb_captouch
5. Verify compilation before upload

### Board Configuration
- **CPU Frequency**: 240 MHz
- **Flash Size**: 16MB
- **PSRAM**: OPI PSRAM
- **Partition Scheme**: Default or "16MB with OTA"
- **USB CDC On Boot**: Enabled (for CAT)
- **USB Mode**: USB-OTG (for MIDI+CDC)

### Upload Procedure
1. Connect board via USB-C
2. Hold BOOT button while clicking Upload (if auto-reset fails)
3. Monitor serial output for initialization messages
4. Test basic functionality before closing IDE

## Troubleshooting

### Common Issues
- **Touch Not Responding**: Check I2C pins (SDA=8, SCL=4), verify GT911 initialization
- **MIDI Not Detected**: Ensure USB Mode = USB-OTG, check TinyUSB configuration
- **CAT Timeout**: Verify CDC port enumeration, check baud rate settings
- **Display Artifacts**: Verify SPI initialization, check bus configuration
- **Encoder Jitter**: Add hardware debouncing, increase interrupt priority

### Debug Tools
- **Serial Monitor**: Enable verbose CAT/MIDI logging
- **MIDI Monitor**: View outgoing MIDI messages
- **Logic Analyzer**: Verify encoder signals, I2C communication
- **Oscilloscope**: Check power supply stability, signal integrity

## Version History

- **v2.0** (Current): ESP32-S3 platform with MIDI+CAT
- **v1.0**: Raspberry Pi Pico platform (see `pico-version` branch)

## Contact and Support

For questions, issues, or contributions, please use GitHub Issues on the project repository.

---

**Remember**: All technical communication in this project MUST be in English to maintain consistency and accessibility for the global developer community.
