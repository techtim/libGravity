# GravityPlus

Alt firmware for the Sitka Instruments **Gravity** eurorack module (Arduino Nano /
ATmega328P), built on the `libGravity` library.

Every one of the six outputs is the **same** gate generator: a Euclidean pattern
(`STEPS` / `HITS`) with an adjustable rotation, per-hit probability, gate length,
phase offset and swing. Set `STEPS = HITS = 1` and a channel becomes a pure
probability gate — so this one generator replaces both the classic Gravity
(probability) and Euclidean firmwares.

The internal clock runs at 96 PPQN for fine-grained duty / offset / clock division.

## Controls

| Input | Action |
|---|---|
| **Encoder press** | Toggle between *selecting* a parameter and *editing* it |
| **Encoder hold + rotate** | Momentary edit of the selected parameter |
| **Encoder rotate** | Move between parameters (or edit, while in edit mode) |
| **BTN1 (PLAY)** | Start / stop the internal clock |
| **SHIFT (BTN2) + PLAY** | Mute — the selected channel, or all channels on the global page |
| **BTN2 + rotate** | Change the selected channel (global page ↔ channels 1–6) |

The top row of the display selects the page: the play/pause icon is the **global
page**; boxes `1`–`6` are the per-channel pages (a muted channel shows `M`).

## Channel page

The channel page shows the channel's Euclidean pattern along the top (3×3 px per
step, filled = hit, outline = rest), then these parameters:

| Param | Range | Notes |
|---|---|---|
| `MOD` (clock mod) | /128 … x24 | Clock division (`/`) or multiplication (`x`); `x1` = quarter note |
| `STEPS` | 1 – 16 | Euclidean pattern length |
| `HITS` | 1 – STEPS | Active hits, spread evenly (Bresenham) |
| `ROTATE` | 0 – (STEPS-1) | Cyclic shift of the pattern (e.g. `X__X_` → `_X__X`) |
| `PROB` | 0 – 100 % | Per-hit trigger probability |
| `DUTY` | 1 – 99 % | Gate length as a percentage of the step |
| `OFFSET` | 0 – 99 % | Phase offset of the gate within the step |
| `SWING` | 50 – 95 % | Swing delay applied to odd steps |
| `CHOKE` | OFF / 1–6 | Silence this channel while the chosen channel's gate is high (self not allowed) |
| `CV1-A` / `CV1-B` / `CV2-A` / `CV2-B` | routing target | Route duplicated CV1 / CV2 to any parameter above (except choke) |

## Global page

`TEMPO`, `RUN` (start/stop from a CV gate), `RESTART` (reset source: none / CV1 /
CV2 / EXT), `SOURCE` (internal, external 24/4/2/1 PPQN, or MIDI clock), `PULSE OUT`
(clock pulse resolution), `ENCODER DIR`, `ROTATE DISP` (flip the screen),
`SAVE` / `LOAD` (10 slots, `A1`–`B5`), `RESET` (restore defaults), the six CV
calibration items, and `ERASE` (full factory reset).

### CV calibration

`CV1/CV2 CAL -5V / 0V / +5V` tune each input so an electrical 0 V reads 0. Patch
the reference voltage, select the matching item, and rotate while watching the
on-screen meter: centre the meter at 0 V and drive it to full deflection at ±5 V.
Calibration is stored in EEPROM.

## CV modulation

`CV1-A` / `CV1-B` / `CV2-A` / `CV2-B` route an input to `CLOCK MOD` or any channel parameter. 
To select destination press-hold-rotate whe while on one of the given parameters.
Bipolar reading (−5…+5 V) is scaled to the target parameter's range (-100%...100%) and added to
its base value, so the routed parameter tracks the CV while the base setting is
preserved. To pick scale press and release (editing mode) on parameter, negative percentage inverts CV value

## Persistence

State auto-saves to a transient EEPROM slot ~2 s after the last change and reloads
on power-up, plus 10 manual `SAVE`/`LOAD` slots. On a firmware update whose stored
layout differs, the module performs a one-time factory reset (see `LAYOUT_REVISION`
in `save_state.cpp`).

## Build & flash

From the repository root (PlatformIO installation required):

```bash
pio run -e gravityplus -t upload
```

Or open `firmware/GravityPlus/GravityPlus.ino` in Arduino IDE and press upload.

To test run:
```bash
pio test -e native -f test_gravityplus
```

## Files

| File | Role |
|---|---|
| `GravityPlus.ino` | setup / loop, clock + UI handlers |
| `channel.h` | the gate generator (pattern, timing, CV routing) + `ChannelPageParam` |
| `app_state.h` | global app state |
| `display.h` | OLED UI (global + channel pages, pattern view, CV meter) |
| `save_state.h/.cpp` | EEPROM persistence + layout-signature reset |
| `clock_mod.h` | shared clock-division tables |
