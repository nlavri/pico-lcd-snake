#ifndef PICO_LCD_PLATFORM_RGB_LED_H
#define PICO_LCD_PLATFORM_RGB_LED_H

#include <stdbool.h>
#include <stdint.h>

bool platform_rgb_led_init(void);
void platform_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue);

#endif
