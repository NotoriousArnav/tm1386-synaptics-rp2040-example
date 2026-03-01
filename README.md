# PS/2 Trackpad (TM1386) Reader for Raspberry Pi Pico

[![Build](https://github.com/NotoriousArnav/pico-touchpad-tm1386/actions/workflows/build.yml/badge.svg)](https://github.com/NotoriousArnav/pico-touchpad-tm1386/actions/workflows/build.yml)

Reads movement and button data from a PS/2 trackpad (TM1386 or compatible) using a Raspberry Pi Pico (RP2040) and prints it over USB serial.

The PS/2 protocol is handled by a **PIO-based driver** (`lib/ps2/`) that can be reused in any pico-sdk project.

Built as a driver/example for using laptop trackpads as input devices in custom projects — in this case, a low-profile DJ jog wheel alternative for scratching and MIDI control.

## Hardware Notes — TM1386

- **Synaptics Absolute Mode:** The driver automatically detects if the trackpad is Synaptics-compatible and switches it into Absolute Mode with W-Mode enabled.
  - This bypasses the trackpad's internal firmware filters, meaning you get raw **X, Y, Z (pressure), and W (finger counts)** instead of pre-processed relative deltas.
  - The physical button now correctly registers even if you press it without a finger on the capacitive surface (a common issue in standard PS/2 mode due to palm-rejection).
- **Multi-Touch:** The TM1386 supports multi-touch presence detection. When in Absolute Mode, the driver outputs `FINGERS:n` indicating if 1, 2, or 3 fingers are touching the pad simultaneously.
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
[INIT] Synaptics Touchpad detected! (v8.1, model 0x1)
[INIT] Enabling Synaptics Absolute + W Mode...
[INIT] Synaptics Absolute Mode enabled.
[INIT] Enabling data reporting (0xF4)...
[INIT] Data reporting enabled.

[READY] Listening for trackpad packets...
[READY] Format: X:nnnn Y:nnnn Z:nnn FINGERS:n BTN:n

X:1452 Y:3200 Z:045 FINGERS:1 BTN:0
X:1460 Y:3205 Z:048 FINGERS:1 BTN:0
X:2800 Y:4000 Z:050 FINGERS:2 BTN:1
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

1. Download the `lib-<commit-sha>-release.zip` file from the [Releases](https://github.com/NotoriousArnav/pico-touchpad-tm1386/releases) page.
2. Extract the zip file directly into the root of your pico-sdk project. This will create a `lib/ps2/` folder.
3. In your top-level `CMakeLists.txt`, add the subdirectory and link it to your target:
   ```cmake
   add_subdirectory(lib/ps2)
   target_link_libraries(your_target ps2)
   ```
4. In your source code:
   ```cpp
   #include "ps2.hpp"

   PS2 device(pio0, 0, /*data_pin=*/2, /*clk_pin=*/3);
   device.init();
   
   // Check if it's a Synaptics multi-touch pad
   uint8_t minor, model, major;
   if (device.synaptics_identify(minor, model, major)) {
       device.synaptics_set_mode(0x81); // Enable Absolute + W Mode
       
       PS2::SynapticsData abs_data;
       if (device.read_synaptics_packet(abs_data)) {
           // use abs_data.x, abs_data.y, abs_data.z, abs_data.w
       }
   } else {
       // Fallback to standard standard PS/2 mouse
       device.reset(id);
       device.enable_reporting();
       
       PS2::Packet rel_data;
       if (device.read_packet(rel_data)) {
           // use rel_data.dx, rel_data.dy, rel_data.left
       }
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
