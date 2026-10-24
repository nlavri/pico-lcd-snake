#include "screen.h"

#if ST7789_240_240_SPOTPEAR
#include "screens/st7789_240_240_spotpear.h"
#define SCREEN_INIT st7789_240_240_spotpear_initScreen
#define SCREEN_FRAME_START st7789_240_240_spotpear_handleFrameStart
#define SCREEN_SCANLINE st7789_240_240_spotpear_handleScanline
#else
#include "screens/st7789_240_135.h"
#define SCREEN_INIT st7789_240_135_initScreen
#define SCREEN_FRAME_START st7789_240_135_handleFrameStart
#define SCREEN_SCANLINE st7789_240_135_handleScanline
#endif

void I_initScreen(void) {
    SCREEN_INIT();
}

void I_handleFrameStart(uint8_t frame) {
    SCREEN_FRAME_START(frame);
}

void I_handleScanline(uint16_t *line, int scanline) {
    SCREEN_SCANLINE(line, scanline);
}

void I_handleFrameEnd(uint8_t frame) {
    (void)frame;
}
