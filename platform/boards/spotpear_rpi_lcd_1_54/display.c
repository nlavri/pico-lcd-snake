#include "platform/display.h"

#include <stddef.h>
#include <string.h>

#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

enum
{
    DISPLAY_WIDTH = 240,
    DISPLAY_HEIGHT = 240,
    LCD_MOSI_PIN = 19,
    LCD_CLOCK_PIN = 18,
    LCD_CHIP_SELECT_PIN = 17,
    LCD_DATA_COMMAND_PIN = 21,
    LCD_RESET_PIN = 22,
    LCD_SPI_BAUDRATE = 10 * 1000 * 1000,
};

static uint16_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
static DisplaySurface surface;

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

static void lcd_select(bool selected)
{
    gpio_put(LCD_CHIP_SELECT_PIN, selected ? 0 : 1);
}

static void lcd_command(uint8_t command, const uint8_t *data, size_t length)
{
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    lcd_select(true);
    gpio_put(LCD_DATA_COMMAND_PIN, 0);
    spi_write_blocking(spi0, &command, 1);
    if (length > 0)
    {
        gpio_put(LCD_DATA_COMMAND_PIN, 1);
        spi_write_blocking(spi0, data, length);
    }
    lcd_select(false);
}

static void lcd_set_window(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    const uint8_t columns[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    const uint8_t rows[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };
    lcd_command(0x2a, columns, sizeof(columns));
    lcd_command(0x2b, rows, sizeof(rows));
}

static void lcd_initialize(void)
{
    spi_init(spi0, LCD_SPI_BAUDRATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_CLOCK_PIN, GPIO_FUNC_SPI);

    gpio_init(LCD_CHIP_SELECT_PIN);
    gpio_set_dir(LCD_CHIP_SELECT_PIN, GPIO_OUT);
    gpio_put(LCD_CHIP_SELECT_PIN, 1);
    gpio_init(LCD_DATA_COMMAND_PIN);
    gpio_set_dir(LCD_DATA_COMMAND_PIN, GPIO_OUT);
    gpio_put(LCD_DATA_COMMAND_PIN, 1);
    gpio_init(LCD_RESET_PIN);
    gpio_set_dir(LCD_RESET_PIN, GPIO_OUT);

    gpio_put(LCD_RESET_PIN, 1);
    sleep_ms(20);
    gpio_put(LCD_RESET_PIN, 0);
    sleep_ms(20);
    gpio_put(LCD_RESET_PIN, 1);
    sleep_ms(100);

    lcd_command(0x01, NULL, 0); // Software reset.
    sleep_ms(150);
    lcd_command(0x11, NULL, 0); // Sleep out.
    sleep_ms(50);
    const uint8_t pixel_format = 0x05; // RGB565.
    lcd_command(0x3a, &pixel_format, 1);
    const uint8_t memory_access = 0x70;
    lcd_command(0x36, &memory_access, 1);
    lcd_set_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    lcd_command(0x21, NULL, 0); // Inversion on.
    lcd_command(0x13, NULL, 0); // Normal display mode.
    lcd_command(0x29, NULL, 0); // Display on.
    sleep_ms(20);
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
    lcd_initialize();
    surface = (DisplaySurface){
        .pixels = framebuffer,
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
    };
    Paint_NewImage(
        (UBYTE *)surface.pixels,
        surface.width,
        surface.height,
        ROTATE_0,
        DISPLAY_COLOR_BLACK);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);
    display_clear(&surface, DISPLAY_COLOR_BLACK);
    display_present(&surface);
    return true;
}

void display_set_brightness(uint8_t percent)
{
    // This HAT drives its backlight directly and exposes no backlight pin.
    (void)percent;
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
        x0, y0, x1, y1, color, (DOT_PIXEL)line_width, DRAW_FILL_EMPTY);
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
        target, x, y, text, DISPLAY_FONT_SMALL, foreground, background);
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
    Paint_DrawString_EN(
        x, y, text, waveshare_font(font), background, foreground);
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
    lcd_set_window(0, 0, target->width - 1, target->height - 1);
    const uint8_t write_memory = 0x2c;
    lcd_command(write_memory, NULL, 0);
    lcd_select(true);
    gpio_put(LCD_DATA_COMMAND_PIN, 1);
    // GUI_Paint stores RGB565 in display byte order, so send raw bytes.
    spi_write_blocking(
        spi0,
        (const uint8_t *)target->pixels,
        (size_t)target->width * target->height * sizeof(uint16_t));
    lcd_select(false);
}
