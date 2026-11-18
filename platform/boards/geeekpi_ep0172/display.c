#include "platform/display.h"

#include <stddef.h>
#include <string.h>

#include "GUI_Paint.h"
#include "common/logger.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "hardware/structs/spi.h"
#include "pico/stdlib.h"

#if PICO_RP2350
#define EP0172_NATIVE_FRAMEBUFFER 1
#else
#define EP0172_NATIVE_FRAMEBUFFER 0
#endif

enum
{
#if EP0172_NATIVE_FRAMEBUFFER
    LOGICAL_WIDTH = 480,
    LOGICAL_HEIGHT = 320,
    DISPLAY_SCALE = 1,
#else
    LOGICAL_WIDTH = 240,
    LOGICAL_HEIGHT = 160,
    DISPLAY_SCALE = 2,
#endif
    PHYSICAL_WIDTH = 480,
    PHYSICAL_HEIGHT = 320,
    LCD_CLOCK_PIN = 2,
    LCD_MOSI_PIN = 3,
    LCD_CHIP_SELECT_PIN = 5,
    LCD_DATA_COMMAND_PIN = 6,
    LCD_RESET_PIN = 7,
    LCD_SPI_BAUDRATE = APP_DISPLAY_SPI_BAUDRATE,
};

_Static_assert(PHYSICAL_WIDTH == LOGICAL_WIDTH * DISPLAY_SCALE,
    "EP-0172 horizontal scale must match its physical width");
_Static_assert(PHYSICAL_HEIGHT == LOGICAL_HEIGHT * DISPLAY_SCALE,
    "EP-0172 vertical scale must match its physical height");

static uint16_t framebuffer[LOGICAL_WIDTH * LOGICAL_HEIGHT];
#if !EP0172_NATIVE_FRAMEBUFFER
static uint8_t line_buffers[2][PHYSICAL_WIDTH * sizeof(uint16_t)]
    __attribute__((aligned(4)));
#endif
static DisplaySurface surface;
static dma_channel_config dma_config;
static int dma_channel = -1;
static volatile bool present_active;
static volatile bool frame_rows_complete;
#if !EP0172_NATIVE_FRAMEBUFFER
static volatile bool row_repeat_active;
static volatile uint16_t logical_row;
static volatile uint8_t active_line_buffer;
#endif
static const uint8_t *present_pixels;
static uint32_t present_started_us;
static uint32_t completed_present_count;
static bool present_reported;

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

static void lcd_reset(void)
{
    gpio_put(LCD_RESET_PIN, 1);
    sleep_ms(20);
    gpio_put(LCD_RESET_PIN, 0);
    sleep_ms(20);
    gpio_put(LCD_RESET_PIN, 1);
    sleep_ms(120);
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
    lcd_reset();

    // ST7796S initialization follows the controller datasheet and the
    // manufacturer-unlock sequence used by Adafruit's ST7796S driver.
    lcd_command(0x01, NULL, 0);
    sleep_ms(150);
    const uint8_t unlock_1 = 0xc3;
    const uint8_t unlock_2 = 0x96;
    const uint8_t vcom = 0x1c;
    const uint8_t memory_access = 0x28; // 480 x 320 landscape, BGR order.
    const uint8_t pixel_format = 0x55; // RGB565.
    const uint8_t interface_control = 0x80;
    const uint8_t inversion_control = 0x00;
    const uint8_t display_function[] = {0x80, 0x02, 0x3b};
    const uint8_t entry_mode = 0xc6;
    const uint8_t lock_1 = 0x69;
    const uint8_t lock_2 = 0x3c;
    lcd_command(0xf0, &unlock_1, 1);
    lcd_command(0xf0, &unlock_2, 1);
    lcd_command(0xc5, &vcom, 1);
    lcd_command(0x36, &memory_access, 1);
    lcd_command(0x3a, &pixel_format, 1);
    lcd_command(0xb0, &interface_control, 1);
    lcd_command(0xb4, &inversion_control, 1);
    lcd_command(0xb6, display_function, sizeof(display_function));
    lcd_command(0xb7, &entry_mode, 1);
    lcd_command(0xf0, &lock_1, 1);
    lcd_command(0xf0, &lock_2, 1);
    lcd_command(0x11, NULL, 0);
    sleep_ms(150);
    lcd_command(0x21, NULL, 0);
    lcd_command(0x13, NULL, 0);
    lcd_command(0x29, NULL, 0);
    sleep_ms(150);
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

#if !EP0172_NATIVE_FRAMEBUFFER
static void expand_row(uint16_t row, uint8_t buffer_index)
{
    const uint8_t *source = present_pixels +
        (size_t)row * LOGICAL_WIDTH * sizeof(uint16_t);
    uint8_t *destination = line_buffers[buffer_index];
    for (uint16_t x = 0; x < LOGICAL_WIDTH; x++)
    {
        const uint8_t high = source[x * 2];
        const uint8_t low = source[x * 2 + 1];
        destination[x * 4] = high;
        destination[x * 4 + 1] = low;
        destination[x * 4 + 2] = high;
        destination[x * 4 + 3] = low;
    }
}

static void start_line_dma(uint8_t buffer_index)
{
    dma_channel_configure(
        (uint)dma_channel,
        &dma_config,
        &spi_get_hw(spi0)->dr,
        line_buffers[buffer_index],
        sizeof(line_buffers[buffer_index]),
        true);
}
#endif

static void display_dma_irq(void)
{
    const uint32_t mask = 1u << (uint)dma_channel;
    if (dma_channel < 0 || (dma_hw->ints0 & mask) == 0)
    {
        return;
    }
    dma_hw->ints0 = mask;

    if (!present_active)
    {
        return;
    }

#if EP0172_NATIVE_FRAMEBUFFER
    // Native RP2350 frames are already stored in the byte order expected by
    // the ST7796S, so the entire image is one DMA transaction.
    frame_rows_complete = true;
    return;
#else
    if (!row_repeat_active)
    {
        row_repeat_active = true;
        start_line_dma(active_line_buffer);
        if (logical_row + 1u < LOGICAL_HEIGHT)
        {
            expand_row((uint16_t)(logical_row + 1u), active_line_buffer ^ 1u);
        }
        return;
    }

    logical_row++;
    if (logical_row >= LOGICAL_HEIGHT)
    {
        frame_rows_complete = true;
        return;
    }

    active_line_buffer ^= 1u;
    row_repeat_active = false;
    start_line_dma(active_line_buffer);
#endif
}

static void initialize_dma(void)
{
    dma_channel = dma_claim_unused_channel(false);
    if (dma_channel < 0)
    {
        LOG_WARNING("display", "DMA unavailable; EP-0172 uses blocking SPI");
        return;
    }

    dma_config = dma_channel_get_default_config((uint)dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, spi_get_dreq(spi0, true));
    dma_channel_set_irq0_enabled((uint)dma_channel, true);
    irq_add_shared_handler(
        DMA_IRQ_0,
        display_dma_irq,
        PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_0, true);
}

static void send_frame_blocking(void)
{
#if EP0172_NATIVE_FRAMEBUFFER
    spi_write_blocking(
        spi0,
        present_pixels,
        (size_t)PHYSICAL_WIDTH * PHYSICAL_HEIGHT * sizeof(uint16_t));
#else
    for (uint16_t row = 0; row < LOGICAL_HEIGHT; row++)
    {
        expand_row(row, 0);
        spi_write_blocking(spi0, line_buffers[0], sizeof(line_buffers[0]));
        spi_write_blocking(spi0, line_buffers[0], sizeof(line_buffers[0]));
    }
#endif
}

bool display_init(void)
{
    lcd_initialize();
    initialize_dma();
    surface = (DisplaySurface){
        .pixels = framebuffer,
        .width = LOGICAL_WIDTH,
        .height = LOGICAL_HEIGHT,
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
    // EP-0172 does not expose the TFT backlight to a Pico GPIO.
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
    if (present_active || target == NULL ||
        target->width != LOGICAL_WIDTH || target->height != LOGICAL_HEIGHT)
    {
        return false;
    }

    lcd_set_window(0, 0, PHYSICAL_WIDTH - 1, PHYSICAL_HEIGHT - 1);
    lcd_command(0x2c, NULL, 0);
    lcd_select(true);
    gpio_put(LCD_DATA_COMMAND_PIN, 1);
    present_pixels = (const uint8_t *)target->pixels;
    present_started_us = time_us_32();

    if (dma_channel < 0)
    {
        send_frame_blocking();
        while (spi_is_busy(spi0))
        {
            tight_loop_contents();
        }
        lcd_select(false);
        return true;
    }

#if EP0172_NATIVE_FRAMEBUFFER
    frame_rows_complete = false;
    present_active = true;
    dma_channel_configure(
        (uint)dma_channel,
        &dma_config,
        &spi_get_hw(spi0)->dr,
        present_pixels,
        (size_t)PHYSICAL_WIDTH * PHYSICAL_HEIGHT * sizeof(uint16_t),
        true);
#else
    logical_row = 0;
    active_line_buffer = 0;
    row_repeat_active = false;
    frame_rows_complete = false;
    present_active = true;
    expand_row(0, 0);
    start_line_dma(0);
#endif
    return true;
}

bool display_present_complete(void)
{
    if (!present_active)
    {
        return true;
    }
    if (!frame_rows_complete || spi_is_busy(spi0))
    {
        return false;
    }

    lcd_select(false);
    present_active = false;
    completed_present_count++;
    if (!present_reported && completed_present_count >= 8u)
    {
        LOG_INFO(
            "display",
            "EP-0172 %s DMA frame in %lu us",
            EP0172_NATIVE_FRAMEBUFFER ? "native" : "scaled",
            (unsigned long)(time_us_32() - present_started_us));
        present_reported = true;
    }
    return true;
}
