#ifndef PICO_LCD_PLATFORM_INPUT_H
#define PICO_LCD_PLATFORM_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct PlatformInputState
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool click;
    bool a;
    bool b;
    bool touch;
    uint16_t touch_x;
    uint16_t touch_y;
} PlatformInputState;

void platform_input_init(void);
PlatformInputState platform_input_read(void);

#endif
