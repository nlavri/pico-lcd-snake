#include "st7789_240_135.h"

static const struct st7789_config lcd_config = {
    .spi      = spi1,
    .gpio_din = 11,
    .gpio_clk = 10,
    .gpio_cs  = 9,
    .gpio_dc  = 8,
    .gpio_rst = 12,
    .gpio_bl  = 13,
};

void st7789_240_135_initScreen(void) {
    // width and height only come into play for fills so let's just pass the memory size instead of LCD size
    st7789_init(&lcd_config, MEMORY_WIDTH, MEMORY_HEIGHT);
    // Clear pixels outside DOOM's rendering window before the first frame.
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
