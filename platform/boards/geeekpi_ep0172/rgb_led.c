#include "platform/rgb_led.h"

#include "pico/status_led.h"

bool platform_rgb_led_init(void)
{
    return status_led_init();
}

void platform_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue)
{
    colored_status_led_set_on_with_color(
        PICO_COLORED_STATUS_LED_COLOR_FROM_RGB(red, green, blue));
}
