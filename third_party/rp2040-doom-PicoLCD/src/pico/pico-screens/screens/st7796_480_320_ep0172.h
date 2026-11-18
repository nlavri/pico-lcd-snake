// GeeekPi/52Pi EP-0172 480x320 ST7796S screen backend.

#ifndef __ST7796_480_320_EP0172__
#define __ST7796_480_320_EP0172__

#include "pico.h"

void st7796_480_320_ep0172_initScreen(void);
void st7796_480_320_ep0172_showBootStage(uint8_t stage);
void st7796_480_320_ep0172_handleFrameStart(uint8_t frame);
void st7796_480_320_ep0172_handleScanline(uint16_t *line, int scanline);
void st7796_480_320_ep0172_handleFrameEnd(uint8_t frame);

#endif
