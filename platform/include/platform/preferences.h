#ifndef PICO_LCD_PLATFORM_PREFERENCES_H
#define PICO_LCD_PLATFORM_PREFERENCES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct PlatformPreferences
{
    uint8_t accent_index;
    uint8_t brightness_percent;
} PlatformPreferences;

bool platform_preferences_load(PlatformPreferences *preferences);
bool platform_preferences_save(const PlatformPreferences *preferences);

#endif
