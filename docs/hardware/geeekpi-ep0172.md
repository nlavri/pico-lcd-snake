# GeeekPi/52Pi EP-0172

The EP-0172 Pico breadboard kit combines a 3.5-inch 480 x 320 SPI TFT, an
analog PSP-style joystick, two buttons, a WS2812-compatible RGB LED, touch,
and several peripherals. This firmware currently supports the display,
joystick, BTN1, BTN2, RGB LED, and GT911 touch panel. The buzzer and two
discrete LEDs are deliberately left unused.

The manufacturer warns that the LCD controller can vary by production batch.
The current driver targets the documented ST7796S/ST7796SU1 controller and
uses the standard ST7796S unlock and initialization sequence. If a later kit
has a different controller, add a separate initialization variant rather than
changing the shared application renderer.

## Pin map

| Function | Pico pin | Firmware use |
| --- | ---: | --- |
| LCD SCK | GP2 | SPI0 SCK |
| LCD MOSI | GP3 | SPI0 TX |
| LCD CS | GP5 | Active-low chip select |
| LCD DC | GP6 | Command/data select |
| LCD reset | GP7 | Active-low reset |
| Touch SDA | GP8 | I2C0 SDA |
| Touch SCL | GP9 | I2C0 SCL |
| Touch reset | GP10 | Active-low reset |
| Touch interrupt | GP11 | Address selection and interrupt input |
| RGB data | GP12 | PIO-driven status LED |
| Buzzer | GP13 | Unused |
| BTN2 | GP14 | B/Back, active low |
| BTN1 | GP15 | Select/Click, active low |
| LED1 | GP16 | Unused |
| LED2 | GP17 | Unused |
| Joystick X | GP26 / ADC0 | Left/right |
| Joystick Y | GP27 / ADC1 | Up/down |

BTN1+BTN2 held for 750 ms is the global Home gesture because this kit exposes
neither a dedicated A button nor a joystick-click switch.

## Display and memory design

A native 480 x 320 RGB565 framebuffer consumes 307,200 bytes, more than the
RP2040's 264 KiB SRAM before the OS and apps are considered. RP2040 builds
therefore keep the project's 240 x 160, 76,800-byte logical framebuffer. The
EP-0172 driver expands each logical pixel and row by two while streaming to the
physical panel. Pico 2 W builds use its larger SRAM for a native 480 x 320
framebuffer, giving apps the full panel resolution.

Two 960-byte scanline buffers are alternated. SPI0 transmits one expanded line
through DMA, repeats it for vertical scaling, and the DMA interrupt prepares
the next line in the other buffer. The panel runs in 480 x 320 landscape mode.
Supplied presets use 62.5 MHz SPI on RP2040 and 75 MHz on RP2350.

The GT911 runs on I2C0 at 400 kHz. Its portrait-oriented coordinates are
rotated into landscape coordinates by the board driver. Touch is exposed as a
pointer event and supplements rather than replaces the physical controls.

The RGB LED is initialized through the Pico SDK status-LED driver, which uses
a small PIO program for the WS2812 protocol. It shows dim blue during boot,
green once startup completes, and red after a fatal startup error.

## Sources

- [EP-0172 vendor wiki](https://wiki.52pi.com/index.php?title=EP-0172)
- [GeeekPi reference firmware](https://github.com/geeekpi/pico_breadboard_kit)
- [Adafruit ST7796S initialization reference](https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7796S.cpp)
