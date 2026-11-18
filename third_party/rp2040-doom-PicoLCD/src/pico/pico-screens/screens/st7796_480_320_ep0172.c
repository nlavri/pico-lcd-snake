#include "st7796_480_320_ep0172.h"

#include <stddef.h>
#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/structs/spi.h"
#include "pico/status_led.h"
#include "pico/stdlib.h"

#include "shared.h"

enum {
    LCD_WIDTH = 480,
    LCD_HEIGHT = 320,
    DIAGNOSTIC_BORDER_HEIGHT = 10,
#if DOOM_EP0172_NATIVE_VIEW
    OUTPUT_WIDTH = SCREENWIDTH,
    OUTPUT_HEIGHT = SCREENHEIGHT,
#elif DOOM_EP0172_BALANCED_VIEW
    OUTPUT_WIDTH = 400,
    OUTPUT_HEIGHT = 250,
#elif DOOM_EP0172_LOW_TEAR_VIEW
    // Preserve DOOM's 8:5 aspect ratio while keeping one RGB565 transfer
    // close to a 60 Hz panel period: 352 * 220 * 16 / 75 MHz = 16.52 ms.
    OUTPUT_WIDTH = 352,
    OUTPUT_HEIGHT = 220,
#else
    OUTPUT_WIDTH = LCD_WIDTH,
    OUTPUT_HEIGHT = 300,
#endif
    OUTPUT_X = (LCD_WIDTH - OUTPUT_WIDTH) / 2,
    OUTPUT_Y = (LCD_HEIGHT - OUTPUT_HEIGHT) / 2,
    LCD_CLOCK_PIN = 2,
    LCD_MOSI_PIN = 3,
    LCD_CHIP_SELECT_PIN = 5,
    LCD_DATA_COMMAND_PIN = 6,
    LCD_RESET_PIN = 7,
#if DOOM_EP0172_DISPLAY_SELF_TEST || DOOM_EP0172_SAFE_SPI
    LCD_SPI_BAUDRATE = 10000000,
#elif DOOM_EP0172_PICO2
    // At the Pico 2 target's 300 MHz system clock, 75 MHz is an exact SPI
    // divisor and is the final controlled overclock step for this panel.
    LCD_SPI_BAUDRATE = 75000000,
#else
    // clk_peri is 270 MHz in the operational build.  62.5 MHz rounded down
    // to only 45 MHz with the RP2040's even prescaler; 67.5 MHz is exact.
    LCD_SPI_BAUDRATE = 67500000,
#endif
};

static uint16_t scaled_line[LCD_WIDTH];
static uint8_t wire_line[DOOM_EP0172_PICO2 ? 2 : 1]
    [LCD_WIDTH * sizeof(uint16_t)] __aligned(4);
static uint16_t output_row;
static bool data_mode;
static uint8_t diagnostic_frame;
#if DOOM_EP0172_PICO2
static dma_channel_config lcd_dma_config;
static int lcd_dma_channel = -1;
static uint8_t next_wire_line;
static bool lcd_dma_active;
#endif

#if DOOM_EP0172_PICO2
static void lcd_wait_for_dma(void)
{
    if (lcd_dma_active) {
        dma_channel_wait_for_finish_blocking((uint)lcd_dma_channel);
        lcd_dma_active = false;
    }
}
#endif

static void lcd_select(bool selected)
{
    gpio_put(LCD_CHIP_SELECT_PIN, selected ? 0 : 1);
}

static void lcd_command(uint8_t command, const uint8_t *data, size_t length)
{
    lcd_select(true);
    gpio_put(LCD_DATA_COMMAND_PIN, 0);
    spi_write_blocking(spi0, &command, 1);
    if (length > 0) {
        gpio_put(LCD_DATA_COMMAND_PIN, 1);
        spi_write_blocking(spi0, data, length);
    }
    lcd_select(false);
}

static void lcd_set_window(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    const uint16_t x_end = (uint16_t)(x + width - 1);
    const uint16_t y_end = (uint16_t)(y + height - 1);
    const uint8_t columns[] = {
        (uint8_t)(x >> 8), (uint8_t)x,
        (uint8_t)(x_end >> 8), (uint8_t)x_end,
    };
    const uint8_t rows[] = {
        (uint8_t)(y >> 8), (uint8_t)y,
        (uint8_t)(y_end >> 8), (uint8_t)y_end,
    };
    lcd_command(0x2a, columns, sizeof(columns));
    lcd_command(0x2b, rows, sizeof(rows));
}

static void lcd_begin_pixels(void)
{
    const uint8_t ram_write = 0x2c;
    lcd_select(true);
    gpio_put(LCD_DATA_COMMAND_PIN, 0);
    spi_write_blocking(spi0, &ram_write, 1);
    gpio_put(LCD_DATA_COMMAND_PIN, 1);
    data_mode = true;
}

static void lcd_write(const uint16_t *pixels, size_t count)
{
    if (!data_mode) {
        lcd_begin_pixels();
    }
    // Match the proven apps driver exactly: keep SPI in 8-bit mode and put
    // each RGB565 pixel on the wire most-significant byte first.  The SDK's
    // 16-bit SPI path works with the ST7789 boards, but did not bring this
    // ST7796S panel out of a blank state.
    const uint8_t line_index =
#if DOOM_EP0172_PICO2
        next_wire_line;
#else
        0;
#endif
    for (size_t pixel = 0; pixel < count; ++pixel) {
        wire_line[line_index][pixel * 2] = (uint8_t)(pixels[pixel] >> 8);
        wire_line[line_index][pixel * 2 + 1] = (uint8_t)pixels[pixel];
    }
#if DOOM_EP0172_PICO2
    // Conversion above overlaps the previous line's DMA transfer.  Wait only
    // when the newly prepared line is ready to enter the SPI FIFO.
    lcd_wait_for_dma();
    dma_channel_configure(
        (uint)lcd_dma_channel,
        &lcd_dma_config,
        &spi_get_hw(spi0)->dr,
        wire_line[line_index],
        count * sizeof(uint16_t),
        true);
    lcd_dma_active = true;
    next_wire_line ^= 1u;
#else
    spi_write_blocking(
        spi0, wire_line[line_index], count * sizeof(uint16_t));
#endif
}

static void lcd_initialize(void)
{
    spi_init(spi0, LCD_SPI_BAUDRATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_CLOCK_PIN, GPIO_FUNC_SPI);

#if DOOM_EP0172_PICO2
    lcd_dma_channel = dma_claim_unused_channel(true);
    lcd_dma_config = dma_channel_get_default_config((uint)lcd_dma_channel);
    channel_config_set_transfer_data_size(&lcd_dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&lcd_dma_config, true);
    channel_config_set_write_increment(&lcd_dma_config, false);
    channel_config_set_dreq(&lcd_dma_config, spi_get_dreq(spi0, true));
#endif

    gpio_init(LCD_CHIP_SELECT_PIN);
    gpio_set_dir(LCD_CHIP_SELECT_PIN, GPIO_OUT);
    lcd_select(false);
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
    sleep_ms(120);

    lcd_command(0x01, NULL, 0);
    sleep_ms(150);
    const uint8_t unlock_1 = 0xc3;
    const uint8_t unlock_2 = 0x96;
    const uint8_t vcom = 0x1c;
    const uint8_t memory_access = 0x28;
    const uint8_t pixel_format = 0x55;
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

static void finish_pixels(void)
{
#if DOOM_EP0172_PICO2
    lcd_wait_for_dma();
#endif
    while (spi_is_busy(spi0)) {
        tight_loop_contents();
    }
#if DOOM_EP0172_PICO2
    while (spi_is_readable(spi0)) {
        (void)spi_get_hw(spi0)->dr;
    }
    spi_get_hw(spi0)->icr = SPI_SSPICR_RORIC_BITS;
#endif
    lcd_select(false);
    data_mode = false;
}

static void fill_display(uint16_t color)
{
    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    for (size_t x = 0; x < LCD_WIDTH; ++x) {
        scaled_line[x] = color;
    }
    for (size_t y = 0; y < LCD_HEIGHT; ++y) {
        lcd_write(scaled_line, LCD_WIDTH);
    }
    finish_pixels();
}

static void clear_display(void)
{
#if DOOM_EP0172_DISPLAY_SELF_TEST
    static const uint16_t diagnostic_colors[] = {
        0xf800, // red
        0x07e0, // green
        0x001f, // blue
        0xffff, // white
        0x0000, // black
        0xf81f, // magenta
    };

    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    for (size_t y = 0; y < LCD_HEIGHT; ++y) {
        const size_t band = y * (sizeof(diagnostic_colors)
            / sizeof(diagnostic_colors[0])) / LCD_HEIGHT;
        for (size_t x = 0; x < LCD_WIDTH; ++x) {
            scaled_line[x] = diagnostic_colors[band];
        }
        lcd_write(scaled_line, LCD_WIDTH);
    }
    finish_pixels();
#else
    fill_display(0x0000);
#endif
}

void st7796_480_320_ep0172_initScreen(void)
{
#if DOOM_EP0172_DISPLAY_SELF_TEST
    // Keep PIO/WS2812 completely out of the first probe-free LCD test.  This
    // log must repeat even if the panel is disconnected.
    printf("EP0172 DIAG 0: entered LCD-only self-test\n");
    stdio_flush();
#elif DOOM_EP0172_RGB_STATUS
    const bool led_ready = status_led_init();
    printf("EP0172 DIAG 1: entry, RGB init=%d\n", led_ready);
    stdio_flush();
    colored_status_led_set_on_with_color(
        PICO_COLORED_STATUS_LED_COLOR_FROM_RGB(0, 0, 24));
    sleep_ms(500);
    colored_status_led_set_state(false);
    sleep_ms(250);
#endif

#if DOOM_EP0172_DIAGNOSTICS
    printf("EP0172 DIAG 2: LCD reset/init at %u Hz\n",
        (unsigned)LCD_SPI_BAUDRATE);
    stdio_flush();
#endif
    lcd_initialize();
#if DOOM_EP0172_RGB_STATUS
    colored_status_led_set_on_with_color(
        PICO_COLORED_STATUS_LED_COLOR_FROM_RGB(24, 16, 0));
#endif
    clear_display();
#if DOOM_EP0172_DISPLAY_SELF_TEST
    static const struct {
        uint16_t pixel;
        const char *name;
    } cycle[] = {
        {0xf800, "red"},
        {0x07e0, "green"},
        {0x001f, "blue"},
        {0xffff, "white"},
        {0xf81f, "magenta"},
    };
    unsigned heartbeat = 0;
    while (true) {
        for (size_t phase = 0; phase < sizeof(cycle) / sizeof(cycle[0]);
             ++phase) {
            printf("EP0172 DIAG 3: heartbeat=%u LCD=%s\n",
                heartbeat++, cycle[phase].name);
            stdio_flush();
            fill_display(cycle[phase].pixel);
            sleep_ms(500);
            sleep_ms(250);
        }
    }
#else
#if DOOM_EP0172_RGB_STATUS
    colored_status_led_set_on_with_color(
        PICO_COLORED_STATUS_LED_COLOR_FROM_RGB(0, 24, 0));
#endif
#endif
}

void st7796_480_320_ep0172_showBootStage(uint8_t stage)
{
#if DOOM_EP0172_DIAGNOSTICS
    enum {
        STAGES_PER_ROW = 16,
        STAGE_PITCH = LCD_WIDTH / STAGES_PER_ROW,
        STAGE_GAP = 3,
        STAGE_WIDTH = STAGE_PITCH - STAGE_GAP,
    };
    if (stage == 0 || stage > STAGES_PER_ROW * 2) {
        return;
    }

    // Both 10-pixel borders are outside the 480x300 game image.  Stages 1-16
    // paint the top green and 17-32 paint the bottom yellow.  Paint only the
    // new block so adjacent calls are cheap and easy to count.
    const bool second_row = stage > STAGES_PER_ROW;
    const uint8_t row_stage = second_row ? stage - STAGES_PER_ROW : stage;
    const uint16_t y = second_row
        ? LCD_HEIGHT - DIAGNOSTIC_BORDER_HEIGHT : 0;
    const uint16_t color = second_row ? 0xffe0 : 0x07e0;
    lcd_set_window((uint16_t)((row_stage - 1) * STAGE_PITCH), y,
        STAGE_WIDTH, DIAGNOSTIC_BORDER_HEIGHT);
    for (size_t x = 0; x < STAGE_WIDTH; ++x) {
        scaled_line[x] = color;
    }
    for (size_t y = 0; y < DIAGNOSTIC_BORDER_HEIGHT; ++y) {
        lcd_write(scaled_line, STAGE_WIDTH);
    }
    finish_pixels();
#else
    (void)stage;
#endif
}

void st7796_480_320_ep0172_handleFrameStart(uint8_t frame)
{
    diagnostic_frame = frame;
    output_row = 0;
#if !DOOM_EP0172_INTERLACED
    lcd_set_window(OUTPUT_X, OUTPUT_Y, OUTPUT_WIDTH, OUTPUT_HEIGHT);
#endif
}

static void scale_horizontal(const uint16_t *source)
{
#if DOOM_EP0172_BALANCED_VIEW || DOOM_EP0172_LOW_TEAR_VIEW
    // Written generically so each progressive preset can preserve the source
    // aspect ratio while the compiler strength-reduces the constant division.
    for (uint16_t x = 0; x < OUTPUT_WIDTH; ++x) {
        scaled_line[x] = source[((uint32_t)x * SCREENWIDTH) / OUTPUT_WIDTH];
    }
#else
    // 320 -> 480 is an exact 3:2 scale: every pair becomes three pixels.
    for (uint16_t pair = 0; pair < SCREENWIDTH / 2; ++pair) {
        const uint16_t first = source[pair * 2];
        const uint16_t second = source[pair * 2 + 1];
        scaled_line[pair * 3] = first;
        scaled_line[pair * 3 + 1] = first;
        scaled_line[pair * 3 + 2] = second;
    }
#endif
}

void st7796_480_320_ep0172_handleScanline(uint16_t *line, int scanline)
{
#if DOOM_EP0172_SCANLINE_TEST
    static const uint16_t bars[] = {
        0xf800, 0xffe0, 0x07e0, 0x07ff, 0x001f, 0xf81f,
    };
    (void)line;
    for (uint16_t x = 0; x < OUTPUT_WIDTH; ++x) {
        uint16_t color = bars[((uint32_t)x * 6) / OUTPUT_WIDTH];
        // A moving white/black checker proves that more than one frame is
        // reaching the panel rather than leaving the first image latched.
        if (((scanline / 16) + (x / 16) + diagnostic_frame) & 1) {
            color ^= 0xffff;
        }
        scaled_line[x] = color;
    }
#elif DOOM_EP0172_NATIVE_VIEW
    lcd_write(line, SCREENWIDTH);
    ++output_row;
    return;
#else
    scale_horizontal(line);
#endif
    const uint16_t next_output_row = (uint16_t)
        (((uint32_t)(scanline + 1) * OUTPUT_HEIGHT) / SCREENHEIGHT);
    while (output_row < next_output_row) {
#if DOOM_EP0172_INTERLACED
        // Update one 150-row field per game frame.  Each selected row gets a
        // one-line window because the ST7796 cannot skip destination rows
        // inside a RAM-write transaction.  The next frame fills the opposite
        // parity, keeping each field below one 60 Hz panel scan at 75 MHz SPI.
        if ((output_row & 1u) == (diagnostic_frame & 1u)) {
            lcd_set_window(OUTPUT_X, (uint16_t)(OUTPUT_Y + output_row),
                OUTPUT_WIDTH, 1);
            lcd_write(scaled_line, OUTPUT_WIDTH);
            finish_pixels();
        }
#else
        lcd_write(scaled_line, OUTPUT_WIDTH);
#endif
        ++output_row;
    }
}

void st7796_480_320_ep0172_handleFrameEnd(uint8_t frame)
{
    (void)frame;
    // handleFrameStart opens one continuous ST7796 RAM-write transaction.
    // Close it after the final scaled row so the next frame can send fresh
    // column/row/window commands and begin its own pixel stream.
    finish_pixels();
}
