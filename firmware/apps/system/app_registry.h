#ifndef PICO_LCD_SYSTEM_APP_REGISTRY_H
#define PICO_LCD_SYSTEM_APP_REGISTRY_H

#include <stddef.h>

#include "app.h"

extern const Application *const application_registry[];
extern const size_t application_registry_count;

#endif
