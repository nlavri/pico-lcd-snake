# DOOM display diagnostics

The normal firmware boots directly to DOOM. Temporary LCD diagnostics can be
useful when adding a display backend or changing its SPI wiring.

## Solid-color transport test

Immediately after `st7789_init()`, display these RGB565 fills for roughly
300–500 ms each:

| Color | RGB565 value |
| --- | --- |
| Red | `0xf800` |
| Green | `0x07e0` |
| Blue | `0x001f` |
| Black/clear | `0x0000` |

Use `st7789_fill()` for each color. A missing image usually indicates power,
reset, chip-select, data/command, clock, or MOSI wiring. Swapped colors indicate
an RGB/BGR configuration issue rather than a DOOM renderer issue.

For startup-stage tracing, temporary fills can also be placed after graphics
initialization, the initial game tic, renderer configuration, and entry into the
main loop. Distinct yellow, cyan, magenta, and white fills were useful while
bringing up these backends, but must not remain in normal firmware because the
last fill can remain visible outside DOOM's rendering window.

## Window-streaming test

Open the exact window used by the backend with `st7789_set_window()`, then
stream one complete row at a time. Fill the top-left, top-right, bottom-left,
and bottom-right quadrants with red, green, blue, and white respectively.

Expected result: four stable rectangular quadrants with straight boundaries.

- Vertical stripes generally mean that the GRAM window is being reopened or
  advanced along the wrong rotated axis.
- A shifted or clipped rectangle indicates an incorrect panel offset or window
  size.
- Correct quadrants followed by corrupt DOOM output points to scaling or frame
  streaming rather than SPI wiring.

The current backends are:

- `st7789_240_135.c` for Waveshare Pico-LCD-1.14.
- `st7789_240_240_spotpear.c` for the SpotPear 1.54-inch HAT.

Remove the temporary colors, delays, diagnostic buffers, and log messages once
the backend passes. Keep a final black fill during initialization so unused
screen margins begin in a known state.
