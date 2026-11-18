#ifndef PICO_LCD_GEEEKPI_EP0172_TOUCH_H
#define PICO_LCD_GEEEKPI_EP0172_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

bool ep0172_touch_init(void);
bool ep0172_touch_read(uint16_t *x, uint16_t *y);

#endif
