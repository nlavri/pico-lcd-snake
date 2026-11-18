// DESCRIPTION:
// LED / LCD screen-specific logic

#ifndef __SCREEN__
#define __SCREEN__

#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif
void I_initScreen(void);

// Probe-free startup diagnostics.  The EP-0172 backend renders each stage in
// the unused top border; other displays intentionally do nothing.
void I_showBootStage(uint8_t stage);

void I_handleFrameStart(uint8_t frame);

void I_handleFrameEnd(uint8_t frame);

void I_handleScanline(uint16_t *line, int scanline);

#ifdef __cplusplus
}
#endif

#endif
