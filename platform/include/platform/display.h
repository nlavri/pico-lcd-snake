#ifndef PICO_LCD_PLATFORM_DISPLAY_H
#define PICO_LCD_PLATFORM_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t DisplayColor;

typedef enum DisplayFont
{
    DISPLAY_FONT_TINY,
    DISPLAY_FONT_SMALL,
    DISPLAY_FONT_COMPACT,
    DISPLAY_FONT_MEDIUM,
    DISPLAY_FONT_LARGE,
} DisplayFont;

enum
{
    DISPLAY_COLOR_BLACK = 0x0000,
    DISPLAY_COLOR_BLUE = 0x001F,
    DISPLAY_COLOR_GREEN = 0x07E0,
    DISPLAY_COLOR_RED = 0xF800,
    DISPLAY_COLOR_GRAY = 0x8430,
    DISPLAY_COLOR_WHITE = 0xFFFF,
};

typedef struct DisplaySurface
{
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
} DisplaySurface;

bool display_init(void);
void display_set_brightness(uint8_t percent);
DisplaySurface *display_acquire_surface(void);
void display_clear(DisplaySurface *surface, DisplayColor color);
void display_draw_point(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    DisplayColor color,
    uint8_t size);
void display_draw_rectangle(
    DisplaySurface *surface,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1,
    DisplayColor color,
    uint8_t line_width);
void display_draw_text(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    const char *text,
    DisplayColor foreground,
    DisplayColor background);
void display_draw_text_with_font(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    const char *text,
    DisplayFont font,
    DisplayColor foreground,
    DisplayColor background);
uint16_t display_text_width(const char *text, DisplayFont font);
uint16_t display_font_height(DisplayFont font);
void display_present(DisplaySurface *surface);
bool display_present_async(DisplaySurface *surface);
bool display_present_complete(void);

#endif
