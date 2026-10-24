// SpotPear 1.54-inch 240x240 ST7789 HAT screen backend.

#ifndef __ST7789_240_240_SPOTPEAR__
#define __ST7789_240_240_SPOTPEAR__

#include "shared.h"
#include "pico.h"
#include "pico/st7789.h"

#define MEMORY_WIDTH 240
#define MEMORY_HEIGHT 240
#define LCD_WIDTH 240
#define LCD_HEIGHT 240

#define SPOTPEAR_OUTPUT_WIDTH 240
// DOOM's 320x200 framebuffer was designed to be displayed at 4:3. 240x180
// preserves that intended display aspect ratio on the square LCD.
#define SPOTPEAR_OUTPUT_HEIGHT 180
#define SPOTPEAR_X_OFFSET ((LCD_WIDTH - SPOTPEAR_OUTPUT_WIDTH) / 2)
#define SPOTPEAR_Y_OFFSET ((LCD_HEIGHT - SPOTPEAR_OUTPUT_HEIGHT) / 2)

void st7789_240_240_spotpear_initScreen(void);
void st7789_240_240_spotpear_handleFrameStart(uint8_t frame);
void st7789_240_240_spotpear_handleScanline(uint16_t *line, int scanline);

#endif
