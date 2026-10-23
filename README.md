# Pico LCD applications

Small application firmware for Raspberry Pi Pico and Pico W with a launcher,
Snake game, and onboard-temperature display.

[1.14inch LCD Display Module](https://www.waveshare.com/wiki/Pico-LCD-1.14) is used both as output display and input joystick.

The planned multi-application FreeRTOS and standalone DOOM structure is
documented in [docs/architecture/README.md](docs/architecture/README.md).

## Build

This project uses Raspberry Pi Pico SDK 2.3.1. The application firmware can be
built for the original RP2040-based Pico or Pico W.

```sh
cmake --preset apps-pico-debug
cmake --build --preset apps-pico-debug
```

For Pico W, use the `apps-picow-debug` preset instead. The Pico firmware is
generated at `build/apps-pico/firmware/apps/pico-apps.uf2`. Hold the Pico's
BOOTSEL button while connecting it over USB, then copy the UF2 file to the
mounted `RPI-RP2` volume.

After the first flash, VS Code's Run and Debug panel provides two workflows:

- `Pico: Upload via USB (no debug)` builds, uploads with `picotool`, and runs.
- `Pico: Debug with probe` builds, uploads over SWD, and starts GDB.

The standalone no-audio DOOM firmware has its own `Doom: Build`,
`Doom: Flash all with probe`, and `Doom: Debug with probe` workflows. See
[firmware/doom/README.md](firmware/doom/README.md) for its separate firmware and
game-data flash layout.

## Controls

The firmware starts in the application launcher. Use up/down to select an app,
A to start or resume it, B to restart it, and joystick click to show or hide
launcher help. Application state is retained while the launcher is visible.
Holding A+B for 750 ms inside an application is the global return-to-launcher
gesture; all joystick controls, including click, remain available to the app.

The Temperature app samples the RP2040 internal sensor twice per second. Its
large reading moves through a violet, blue, cyan, green, yellow, orange, and red
color spectrum as the measured temperature rises.
