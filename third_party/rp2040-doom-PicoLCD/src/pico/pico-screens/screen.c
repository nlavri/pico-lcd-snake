#include "screen.h"

static bool screen_initialized;

#if ST7796_480_320_EP0172
#include "screens/st7796_480_320_ep0172.h"
#define SCREEN_INIT st7796_480_320_ep0172_initScreen
#define SCREEN_FRAME_START st7796_480_320_ep0172_handleFrameStart
#define SCREEN_SCANLINE st7796_480_320_ep0172_handleScanline
#define SCREEN_FRAME_END st7796_480_320_ep0172_handleFrameEnd
#elif ST7789_240_240_SPOTPEAR
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
    if (screen_initialized) {
        return;
    }
    SCREEN_INIT();
    screen_initialized = true;
}

void I_showBootStage(uint8_t stage) {
#if ST7796_480_320_EP0172
    st7796_480_320_ep0172_showBootStage(stage);
#else
    (void)stage;
#endif
}

void I_handleFrameStart(uint8_t frame) {
    SCREEN_FRAME_START(frame);
}

void I_handleScanline(uint16_t *line, int scanline) {
    SCREEN_SCANLINE(line, scanline);
}

void I_handleFrameEnd(uint8_t frame) {
#if ST7796_480_320_EP0172
    SCREEN_FRAME_END(frame);
#else
    (void)frame;
#endif
}
