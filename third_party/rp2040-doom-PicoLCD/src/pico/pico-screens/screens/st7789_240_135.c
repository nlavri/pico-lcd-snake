#include "st7789_240_135.h"
#include "pico/stdlib.h"

static const struct st7789_config lcd_config = {
    .spi      = spi1,
    .gpio_din = 11,
    .gpio_clk = 10,
    .gpio_cs  = 9,
    .gpio_dc  = 8,
    .gpio_rst = 12,
    .gpio_bl  = 13,
};

static uint16_t diagnostic_line[DOWNSAMPLED_WIDTH];

void st7789_240_135_initScreen(void) {
    // width and height only come into play for fills so let's just pass the memory size instead of LCD size
    st7789_init(&lcd_config, MEMORY_WIDTH, MEMORY_HEIGHT);

    // Short power-on diagnostic: it proves that the newly built firmware was
    // flashed and isolates LCD transport problems from DOOM renderer faults.
    st7789_fill(0xf800);
    sleep_ms(500);
    st7789_fill(0x07e0);
    sleep_ms(500);
    st7789_fill(0x001f);
    sleep_ms(500);
    st7789_fill(0x0000);

    // Exercise the same full-window, row-streaming path used by DOOM. The
    // expected image is a centered landscape rectangle with red/green on the
    // top half and blue/white on the bottom half.
    st7789_set_window(40 + SCREEN_WIDTH_OFFSET, 53,
                      DOWNSAMPLED_WIDTH, DOWNSAMPLED_HEIGHT);
    for (uint16_t y = 0; y < DOWNSAMPLED_HEIGHT; ++y) {
        for (uint16_t x = 0; x < DOWNSAMPLED_WIDTH; ++x) {
            if (y < DOWNSAMPLED_HEIGHT / 2) {
                diagnostic_line[x] = x < DOWNSAMPLED_WIDTH / 2
                    ? 0xf800 : 0x07e0;
            } else {
                diagnostic_line[x] = x < DOWNSAMPLED_WIDTH / 2
                    ? 0x001f : 0xffff;
            }
        }
        st7789_write(diagnostic_line, sizeof(diagnostic_line));
    }
    sleep_ms(1500);
    st7789_fill(0x0000);
}

void st7789_240_135_handleFrameStart(uint8_t frame) {
    nearestNeighborHandleFrameStart();

    // The Pico-LCD-1.14 is a 240x135 viewport inside the ST7789's 320x240
    // GRAM.  With MADCTL configured for landscape, open the complete output
    // area once and stream rows into it.  Reopening a 216x1 GRAM window for
    // every logical row makes the rotated controller advance along the wrong
    // axis, producing a portrait block of vertical stripes.
    const uint16_t x = 40 + SCREEN_WIDTH_OFFSET;
    const uint16_t y = 53;
    st7789_set_window(x, y, DOWNSAMPLED_WIDTH, DOWNSAMPLED_HEIGHT);
}

void st7789_240_135_blit(uint16_t *downsampled_line, int scanline) {
    (void)scanline;
    st7789_write(downsampled_line, DOWNSAMPLED_WIDTH * sizeof(uint16_t));
}

void st7789_240_135_handleScanline(uint16_t *line, int scanline) {
    nearestNeighborHandleDownsampling(line, scanline, st7789_240_135_blit);
}
