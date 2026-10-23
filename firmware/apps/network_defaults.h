#ifndef PICO_LCD_NETWORK_DEFAULTS_H
#define PICO_LCD_NETWORK_DEFAULTS_H

#if defined(__has_include)
#if __has_include("network_defaults.local.h")
#include "network_defaults.local.h"
#endif
#endif

#ifndef PICO_LCD_DEFAULT_WIFI_SSID
#define PICO_LCD_DEFAULT_WIFI_SSID ""
#endif

#ifndef PICO_LCD_DEFAULT_WIFI_PASSWORD
#define PICO_LCD_DEFAULT_WIFI_PASSWORD ""
#endif

#endif
