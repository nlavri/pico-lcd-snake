#ifndef PICO_LCD_PLATFORM_TIME_H
#define PICO_LCD_PLATFORM_TIME_H

#include <stdint.h>

void platform_sleep_ms(uint32_t milliseconds);
uint32_t platform_time_ms(void);
uint32_t platform_time_us(void);
uint32_t platform_random_u32(void);

#endif
