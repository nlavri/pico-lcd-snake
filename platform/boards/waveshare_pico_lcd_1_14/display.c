#include "platform/display.h"

#include <stddef.h>
#include <string.h>

#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in14.h"
#include "rp2040/spi_dma.h"

static uint16_t framebuffer[LCD_1IN14_WIDTH * LCD_1IN14_HEIGHT];
static DisplaySurface surface;
static bool present_active;

static sFONT *waveshare_font(DisplayFont font)
{
    switch (font)
    {
    case DISPLAY_FONT_TINY:
        return &Font8;
    case DISPLAY_FONT_COMPACT:
        return &Font16;
    case DISPLAY_FONT_MEDIUM:
        return &Font20;
    case DISPLAY_FONT_LARGE:
        return &Font24;
    case DISPLAY_FONT_SMALL:
    default:
        return &Font12;
    }
}

static void clear_rgb565_buffer(DisplaySurface *target, DisplayColor color)
{
    uint8_t *bytes = (uint8_t *)target->pixels;
    const uint8_t high_byte = (uint8_t)(color >> 8);
    const uint8_t low_byte = (uint8_t)color;
    const uint32_t pixel_count = (uint32_t)target->width * target->height;

    for (uint32_t i = 0; i < pixel_count; i++)
    {
        bytes[i * 2] = high_byte;
        bytes[i * 2 + 1] = low_byte;
    }
}

bool display_init(void)
{
    if (DEV_Module_Init() != 0)
    {
        return false;
    }

    DEV_SET_PWM(50);
    LCD_1IN14_Init(HORIZONTAL);

    surface = (DisplaySurface){
        .pixels = framebuffer,
        .width = LCD_1IN14.WIDTH,
        .height = LCD_1IN14.HEIGHT,
    };

    Paint_NewImage(
        (UBYTE *)surface.pixels,
        surface.width,
        surface.height,
        ROTATE_0,
        DISPLAY_COLOR_GRAY);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);
    clear_rgb565_buffer(&surface, DISPLAY_COLOR_GRAY);
    LCD_1IN14_Display(surface.pixels);
    DEV_Delay_ms(100);

    return true;
}

void display_set_brightness(uint8_t percent)
{
    DEV_SET_PWM(percent > 100 ? 100 : percent);
}

DisplaySurface *display_acquire_surface(void)
{
    return &surface;
}

void display_clear(DisplaySurface *target, DisplayColor color)
{
    clear_rgb565_buffer(target, color);
}

void display_draw_point(
    DisplaySurface *target,
    uint16_t x,
    uint16_t y,
    DisplayColor color,
    uint8_t size)
{
    (void)target;
    Paint_DrawPoint(x, y, color, (DOT_PIXEL)size, DOT_FILL_RIGHTUP);
}

void display_draw_rectangle(
    DisplaySurface *target,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1,
    DisplayColor color,
    uint8_t line_width)
{
    (void)target;
    Paint_DrawRectangle(
        x0,
        y0,
        x1,
        y1,
        color,
        (DOT_PIXEL)line_width,
        DRAW_FILL_EMPTY);
}

void display_draw_text(
    DisplaySurface *target,
    uint16_t x,
    uint16_t y,
    const char *text,
    DisplayColor foreground,
    DisplayColor background)
{
    display_draw_text_with_font(
        target,
        x,
        y,
        text,
        DISPLAY_FONT_SMALL,
        foreground,
        background);
}

void display_draw_text_with_font(
    DisplaySurface *target,
    uint16_t x,
    uint16_t y,
    const char *text,
    DisplayFont font,
    DisplayColor foreground,
    DisplayColor background)
{
    (void)target;

    // The Waveshare helper reverses its two color arguments internally.
    Paint_DrawString_EN(
        x,
        y,
        text,
        waveshare_font(font),
        background,
        foreground);
}

uint16_t display_text_width(const char *text, DisplayFont font)
{
    return (uint16_t)(strlen(text) * waveshare_font(font)->Width);
}

uint16_t display_font_height(DisplayFont font)
{
    return waveshare_font(font)->Height;
}

void display_present(DisplaySurface *target)
{
    while (!display_present_async(target))
    {
        display_present_complete();
        tight_loop_contents();
    }
    while (!display_present_complete())
    {
        tight_loop_contents();
    }
}

bool display_present_async(DisplaySurface *target)
{
    if (present_active || target == NULL)
    {
        return false;
    }

    LCD_1IN14_SetWindows(0, 0, target->width, target->height);
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    present_active = platform_spi_dma_start(
        spi1,
        (const uint8_t *)target->pixels,
        (size_t)target->width * target->height * sizeof(uint16_t));
    if (!present_active)
    {
        DEV_Digital_Write(EPD_CS_PIN, 1);
    }
    return true;
}

bool display_present_complete(void)
{
    if (!present_active)
    {
        return true;
    }
    if (!platform_spi_dma_is_complete(spi1))
    {
        return false;
    }

    DEV_Digital_Write(EPD_CS_PIN, 1);
    present_active = false;
    return true;
}
