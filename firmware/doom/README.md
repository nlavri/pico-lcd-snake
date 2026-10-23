# DOOM Firmware

This is the integration guide for the standalone DOOM firmware product. Its
source is vendored at `third_party/rp2040-doom-PicoLCD`; `pico-extras` is also
vendored so the build does not depend on either upstream repository remaining
available.

DOOM is intentionally separate from the normal application firmware because it
requires its own optimized runtime, linker and flash layout, build type, and
potentially different SDK/toolchain constraints.

The selected target is `doom_tiny_ST7789_240_135`, adapted for Pico SDK 2.3.1.
Audio is disabled, while the Waveshare Pico LCD 1.14-inch ST7789 display and
onboard controls are enabled.

## Build

In VS Code, run `Doom: Build` from **Terminal > Run Build Task**. The task
configures the independent project and produces:

- `build/doom-pico/src/doom_tiny_ST7789_240_135.elf`
- `build/doom-pico/src/doom_tiny_ST7789_240_135.uf2`

The compressed shareware game data is
`third_party/rp2040-doom-PicoLCD/doom1.whx`. It is deliberately separate from
the firmware and must be written to flash address `0x10041000`.

## Install and debug

With the second Pico configured as a CMSIS-DAP debug probe and connected over
SWD, run the `Doom: Flash all with probe` task. It writes both the ELF and WHX,
verifies them, and starts the target. This is the normal button-free install
path.

The Run and Debug panel also contains `Doom: Debug with probe`. Its preparation
task rebuilds DOOM and ensures the WHX data is present before starting GDB and
loading the ELF.

For a first BOOTSEL install without a probe, copy the generated UF2 and then
load the game data before starting the program:

```sh
picotool load -F -v build/doom-pico/src/doom_tiny_ST7789_240_135.uf2
picotool load -v -x -t bin third_party/rp2040-doom-PicoLCD/doom1.whx -o 0x10041000
```

Because this DOOM target does not expose a USB reset interface, later
button-free updates should use the SWD probe.

## Controls

| Control | Game | Menu |
| --- | --- | --- |
| Joystick directions | Move/turn | Navigate |
| Joystick click | Enter | Select |
| A | Fire | — |
| B | Use/open | — |
| A+B | Open menu | Close/back |

Inputs are active-low and debounced in the DOOM platform input adapter.
