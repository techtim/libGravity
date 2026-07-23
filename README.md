# Sitka Instruments Gravity Firmware Abstraction

This library helps make writing firmware easier by abstracting away the initialization and peripheral interactions. Now your firmware code can just focus on the logic and behavior of the app, and keep the low level code neatly tucked away in this library.

## Quick Start

You can flash the firmware to your module using the [Web Installer](https://awonak.github.io/alt-gravity/). This website also provides demo videos and documentation for each firmware version.

https://awonak.github.io/alt-gravity/

## Installation

Download or git clone this repository into your Arduino > libraries folder.

Common directory locations:

* [Windows] `C:\Users\{username}\Documents\Arduino\libraries`
* [Mac] `/Users/{username}/Documents/Arduino/libraries`
* [Linux] `~/Arduino/libraries`

## Required Third-party Libraries

* [uClock](https://github.com/midilab/uClock) [MIT] - (Included with this repo) Handle clock tempo, external clock input, and internal clock timer handler.
* [RotateEncoder](https://github.com/mathertel/RotaryEncoder) [BSD] - Library for reading and interpreting encoder rotation.
* [U8g2](https://github.com/olikraus/u8g2/) [MIT] - Graphics helper library.
* [NeoHWSerial](https://github.com/gicking/NeoHWSerial) [MIT] - Drop-in replacement for the Arduino built-in class HardwareSerial

## Repository Layout

```
src/                     libGravity hardware abstraction library
  libGravity.h/.cpp        Gravity umbrella class (display, clock, I/O objects)
  analog_input.h           AnalogInput  - CV inputs (calibration, gate/edge detect)
  button.h                 Button       - debounced buttons with press/long-press
  digital_output.h         DigitalOutput- gate/trigger + LED outputs
  encoder.h                Encoder      - rotary encoder (wraps RotaryEncoder)
  clock.h                  Clock        - tempo/source wrapper around uClock + MIDI
  peripherials.h           Arduino pin map for the Gravity module
  uClock/                  Vendored uClock library (AVR timer clock engine)
firmware/
  Gravity/                 Alt firmware: probability / duty / offset / swing channels
  Euclidean/               Alt firmware: Euclidean rhythm generator channels
examples/                  Small standalone sketches demonstrating the library
test/                      Host unit tests (PlatformIO + ArduinoFake, see below)
  native_compat/           Host-only shims for AVR headers (util/atomic, pgmspace)
```

The peripheral classes in `src/` are header-only and depend only on the Arduino
core, so they can be unit-tested on the host. Each firmware's `channel.h` includes
`digital_output.h` directly (rather than the full `<libGravity.h>` umbrella, which
pulls in U8g2 / NeoHWSerial / uClock) so the pure channel logic compiles and tests
off-target.

## Example

Here's a trivial example showing some of the ways to interact with the library. This script rotates the active clock channel according to the set tempo. The encoder can change the temo or rotation direction. The play/pause button will toggle the clock activity on or off. The shift button will freeze the clock from advancing the channel rotation.

```cpp
#include "gravity.h"

byte idx = 0;
bool reversed = false;
bool freeze = false;
byte selected_param = 0;

// Initialize the gravity library and attach your handlers in the setup method.
void setup() {
    // Initialize Gravity.
    gravity.Init();

    // Attach handlers.
    gravity.clock.AttachIntHandler(IntClock);
    gravity.encoder.AttachRotateHandler(HandleRotate);
    gravity.encoder.AttachPressHandler(ChangeSelectedParam);
    gravity.play_button.AttachPressHandler(HandlePlayPressed);

    // Initial state.
    gravity.outputs[idx].High();
}

// The loop method must always call `gravity.Process()` to read any peripherial state changes.
void loop() {
    gravity.Process();
    freeze = gravity.shift_button.On();
    UpdateDisplay();
}

// The rest of the code is your apps logic!

void IntClock(uint32_t tick) {
    if (tick % 12 == 0  && ! freeze) {
        gravity.outputs[idx].Low();
        if (reversed) {
            idx = (idx == 0) ? OUTPUT_COUNT - 1 : idx - 1;
        } else {
            idx = (idx + 1) % OUTPUT_COUNT;
        }
        gravity.outputs[idx].High();
    }
}

void HandlePlayPressed() {
    gravity.clock.Pause();
    if (gravity.clock.IsPaused()) {
        for (int i = 0; i < OUTPUT_COUNT; i++) {
            gravity.outputs[i].Low();
        }
    }
}

void HandleRotate(Direction dir, int val) {
    if (selected_param == 0) {
        gravity.clock.SetTempo(gravity.clock.Tempo() + val);
    } else if (selected_param == 1) {
        reversed = (dir == DIRECTION_DECREMENT);
    }
}

void ChangeSelectedParam() {
    selected_param = (selected_param + 1) % 2;
}

void UpdateDisplay() {
    gravity.display.clearDisplay();

    if (freeze) {
        gravity.display.setCursor(42, 30);
        gravity.display.print("FREEZE!");
        gravity.display.display();
        return;
    }

    gravity.display.setCursor(10, 0);
    gravity.display.print("Tempo: ");
    gravity.display.print(gravity.clock.Tempo());

    gravity.display.setCursor(10, 10);
    gravity.display.print("Direction: ");
    gravity.display.print((reversed) ? "Backward" : "Forward");

    gravity.display.drawChar(0, selected_param * 10, 0x10, 1, 0, 1);

    gravity.display.display();
}
```

### Build for release

```
$ arduino-cli compile -v -b  arduino:avr:nano ./firmware/Gravity/Gravity.ino -e --output-dir=./build/
```

## Testing

Unit tests run on the host (no board required) using
[PlatformIO](https://platformio.org/) with the [ArduinoFake](https://github.com/FabioBatSilva/ArduinoFake)
mock of the Arduino core and the Unity test framework. AVR-only headers used by the
firmware (`<util/atomic.h>`, `<avr/pgmspace.h>`) are satisfied by the host-only shims
in `test/native_compat/`, so the real target build still uses the genuine headers.

Install PlatformIO Core once:

```
$ pip install --upgrade platformio
```

Run the host unit tests:

```
$ pio test -e native
```

Build the firmware for an Arduino Nano (the `nano` env; set `board = nanoatmega328`
in `platformio.ini` for older 57600-baud bootloader Nanos):

```
$ pio run -e nano                              # builds firmware/Euclidean
$ PLATFORMIO_SRC_DIR=firmware/Gravity pio run -e nano   # builds firmware/Gravity
$ pio run -e nano -t upload                     # flash the connected board
```

### What's covered

| Suite | Under test |
| --- | --- |
| `test/test_pattern` | Euclidean rhythm generation (`Pattern`) |
| `test/test_channel` | Channel clock-mod / steps / hits clamping and mute |
| `test/test_digital_output` | Gate/trigger state and trigger-duration release |
| `test/test_button` | Debounce, press, and long-press callbacks |
| `test/test_analog_input` | CV mapping, attenuation, and rising-edge detection |

The interrupt-driven `Clock` (uClock + serial MIDI) and the U8g2 display are not
host-tested; verify those on hardware. To add a suite, drop a new
`test/test_<name>/test_<name>.cpp` and PlatformIO will build and run it.

### Continuous integration

`.github/workflows/ci.yml` runs the native unit tests and compiles both firmwares
for the Arduino Nano on every push and pull request.

Pushing a version tag (e.g. `git tag v2.0.2 && git push origin v2.0.2`) additionally
builds the firmware and publishes a GitHub Release with the compiled `.hex` files
attached (`gravity-euclidean-<tag>.hex` and `gravity-classic-<tag>.hex`).