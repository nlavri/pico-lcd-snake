// DESCRIPTION:
// LED / LCD screen-specific logic

#ifndef __SCREEN__
#define __SCREEN__

#include "pico.h"

#ifdef __cplusplus
extern "C" 
#endif
void I_initScreen(void);

#ifdef __cplusplus
extern "C" 
#endif
void I_handleFrameStart(uint8_t frame);

#ifdef __cplusplus
extern "C" 
#endif
void I_handleFrameEnd(uint8_t frame);

#ifdef __cplusplus
extern "C" 
#endif
void I_handleScanline(uint16_t *line, int scanline);

#endif
