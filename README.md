# PS/2 Trackpad (TM1386) Reader for Raspberry Pi Pico

[![Build](https://github.com/NotoriousArnav/pico-touchpad-tm1386/actions/workflows/build.yml/badge.svg)](https://github.com/NotoriousArnav/pico-touchpad-tm1386/actions/workflows/build.yml)

Reads movement and button data from a PS/2 trackpad (TM1386 or compatible) using a Raspberry Pi Pico (RP2040) and prints it over USB serial.

The PS/2 protocol is handled by a **PIO-based driver** (`lib/ps2/`) that can be reused in any pico-sdk project.

Built as a driver/example for using laptop trackpads as input devices in custom projects — in this case, a low-profile DJ jog wheel alternative for scratching and MIDI control.

## Hardware Notes — TM1386

- **Single physical button** — reports as PS/2 middle-click (bit 2 of byte 0), not left-click. The application remaps it to a unified `BTN` field.
- **LED / notch** at the top-left corner of the module — this is a physical reference point only and has no electrical function.
- **Orientation:** Mount the trackpad in portrait (rotated 90° clockwise from default) so the X axis becomes the long scratch axis and the Y axis becomes the short axis for secondary controls.

## Wiring

```
                              Raspberry Pi Pico
                             ┌──────────────────┐
  ┌──────────┐               │                  │
  │ Trackpad │               │   3.3V (Pin 36) ─┼──┬─────┬──────┐
  │          │               │                  │ 4.7k  4.7k    │
  │  DATA ───┼───────────────┼── GP2 (Pin 4)  ──┼──┘     │      │
  │  CLK  ───┼───────────────┼── GP3 (Pin 5)  ──┼────────┘      │
  │  VCC  ───┼───────────────┼──────────────────┼───────────────┘
  │  GND  ───┼───────────────┼── GND (Pin 38)   │
  └──────────┘               └──────────────────┘
```

- **GP2** = DATA, **GP3** = CLK (must be adjacent pins)
- **4.7k-10k pull-up resistors** on DATA and CLK to 3.3V (PS/2 is open-drain). 4.7k tested and working.
- Powered from the Pico's 3V3 OUT pin. If your trackpad doesn't respond at 3.3V, try powering it from VBUS (5V) with a level shifter on DATA/CLK.

## Pre-built Firmware

Pre-built `.uf2` files are available on the [Releases](https://github.com/NotoriousArnav/pico-touchpad-tm1386/releases) page. Each push to `main` automatically builds and publishes a new release named `release-<commit>-build.uf2`.

Download the latest `.uf2`, hold **BOOTSEL** on the Pico, plug it in, and copy the file to the **RPI-RP2** drive.

## Building

Requirements: `arm-none-eabi-gcc`, `cmake`, `pico-sdk` (v2.x).

```bash
export PICO_SDK_PATH=~/Code/pico-sdk   # adjust to your path

mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

This produces `build/main.uf2` (~73 KB).

## Flashing

1. Hold **BOOTSEL** on the Pico and plug it into USB.
2. Copy `build/main.uf2` to the **RPI-RP2** drive that appears.
3. The Pico reboots automatically.

## Usage

Open a serial terminal at 115200 baud:

```bash
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

You should see:

```
===========================================
  PS/2 Trackpad TM1386 — Pico USB Reader
===========================================
DATA pin: GP2
CLK  pin: GP3

[INIT] Initializing PS/2 PIO driver...
[INIT] PIO driver ready.
[INIT] Sending reset (0xFF)...
[INIT] Reset OK. Device ID: 0x00
[INIT] Enabling data reporting (0xF4)...
[INIT] Data reporting enabled.

[READY] Listening for trackpad packets...
[READY] Format: X:±nnn Y:±nnn BTN:n

X:  +3 Y:  -1 BTN:0
X:  +5 Y:  -2 BTN:0
X:  +0 Y:  +1 BTN:1
```

### LED indicators

| Pattern | Meaning |
|---------|---------|
| Blinking (boot) | Waiting for USB serial connection |
| 2 slow blinks | Reset succeeded |
| 5 fast blinks | Reset failed (continuing anyway) |
| Toggling | Receiving packets |

## Troubleshooting

- **No output at all** -- Make sure you're connected to `/dev/ttyACM0` (USB CDC), not a UART device.
- **Reset fails** -- Some trackpads auto-initialize and don't respond to 0xFF. The code retries 3 times and continues regardless.
- **No packets** -- Check pull-up resistors are in place. Verify CLK and DATA pins match your wiring. Try swapping to 5V power + level shifter.
- **Garbage data / `[OVF]` flags** -- Likely a packet desync. The driver actively resyncs by scanning for valid byte-0 headers (bit 3 set). If you see `[PS2] Resync` messages, the recovery is working. A Pico reset clears persistent issues.
- **Button shows as middle-click in raw PS/2** -- This is normal for the TM1386. The application remaps it to `BTN`.

## Reusing the PS/2 Library

The `lib/ps2/` directory is a self-contained, reusable PS/2 host driver. To use it in another pico-sdk project:

1. Copy the `lib/ps2/` folder into your project.
2. In your top-level `CMakeLists.txt`:
   ```cmake
   add_subdirectory(lib/ps2)
   target_link_libraries(your_target ps2)
   ```
3. In your source:
   ```cpp
   #include "ps2.hpp"

   PS2 device(pio0, 0, /*data_pin=*/2, /*clk_pin=*/3);
   device.init();
   device.reset(id);
   device.enable_reporting();

   PS2::Packet pkt;
   if (device.read_packet(pkt)) {
       // use pkt.dx, pkt.dy, pkt.left, pkt.right, pkt.middle
   }
   ```

## Project Structure

```
pico-touchpad-tm1386/
├── CMakeLists.txt              # Top-level build configuration
├── pico_sdk_import.cmake       # Pico SDK import helper
├── LICENSE                     # GPL-3.0-or-later
├── README.md
├── lib/ps2/                    # Reusable PS/2 driver library
│   ├── CMakeLists.txt          # INTERFACE library, auto-generates PIO header
│   ├── ps2.pio                 # PIO assembly for receiving PS/2 frames
│   ├── ps2.hpp                 # Public API: PS2 class
│   └── ps2.cpp                 # Implementation: PIO receive, bit-bang transmit
└── src/
    └── main.cpp                # Application entry point
```

## Future Plans

- **USB MIDI output** -- Pico appears as a USB MIDI device. X axis mapped to one MIDI CC (scratch/jog), Y axis to another (pitch bend / effects). Button sends a MIDI note or CC toggle.
- **Scroll wheel support** -- IntelliMouse extension (4-byte packets) for additional control axis.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE) for full text.
