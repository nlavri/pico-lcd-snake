#include "platform/rgb_led.h"

bool platform_rgb_led_init(void)
{
    return false;
}

void platform_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue)
{
    (void)red;
    (void)green;
    (void)blue;
}
