# RP2040 DOOM port

This is a trimmed, locally vendored copy of the RP2040 DOOM port used by the
standalone `doom-pico` firmware. It supports three displays and builds without
audio:

- Waveshare Pico-LCD-1.14, 240x135.
- SpotPear Raspberry Pi Game LCD 1.54-inch HAT, 240x240.
- GeeekPi/52Pi EP-0172 ST7796S kit, 480x320.

The snapshot, upstream commit, and local modifications are recorded in
`UPSTREAM.md`. Licensing for the vendored code is in `COPYING.md`.

## Build

Use `Doom: Configure` followed by the display-specific VS Code build task, or
run one of:

```sh
cmake --build build/doom-pico --target doom_tiny_ST7789_240_135 -j 6
cmake --build build/doom-pico --target doom_tiny_ST7789_240_240_SPOTPEAR -j 6
cmake --build build/doom-pico2w-ep0172 --target doom_tiny_ST7796_480_320_EP0172 -j 6
```

The ELF and UF2 files are created under `build/doom-pico/src/`. The separate
`doom1.whx` data image is loaded at flash offset `0x41000` by the repository's
flash tasks.

The SpotPear target has a matching VS Code Run and Debug entry named
`Doom SpotPear: Release via USB (keep game data)`. It updates only the firmware
region and preserves an existing WHX image.

The EP-0172 target has a matching `Doom EP-0172: Release via USB (keep game
data)` entry. Its tested Pico 2 configuration scales DOOM's 320x200 scene to a
centered 400x250 progressive image and streams it without a framebuffer. A
Pico W runs this target as a plain RP2040, with Wi-Fi disabled.

`src/whd_gen` is retained so a different compatible WAD can be converted to a
new WHD/WHX image. Enable its host-side CMake target with
`DOOM_BUILD_WHD_GEN=ON`. The old generated `tiny.whd.h` C-array is deliberately
not stored because device builds read the much smaller `.whx` file directly
from flash.

## Controls

- Joystick: movement and menu navigation
- Joystick click: menu selection / Enter
- A: fire
- B: use during gameplay; back one level in menus
- A+B: open or close the menu

On EP-0172, BTN1 replaces both joystick-click in menus and A during gameplay;
BTN2 is B. Hold BTN1+BTN2 to open or close the menu.
