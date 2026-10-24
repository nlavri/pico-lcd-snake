#include "st7789_240_240_spotpear.h"

static const struct st7789_config lcd_config = {
    .spi      = spi0,
    .gpio_din = 19,
    .gpio_clk = 18,
    .gpio_cs  = 17,
    .gpio_dc  = 21,
    .gpio_rst = 22,
    .gpio_bl  = -1,
};

static uint16_t scaled_line[SPOTPEAR_OUTPUT_WIDTH];
static uint16_t blurred_edge[SPOTPEAR_OUTPUT_WIDTH];
static uint16_t ambient_line[SPOTPEAR_OUTPUT_WIDTH];
static uint16_t current_output_row;
static bool update_ambient_bands;

#define AMBIENT_BAND_HEIGHT SPOTPEAR_Y_OFFSET
#define AMBIENT_BLUR_RADIUS 5
#define AMBIENT_UPDATE_MASK 3

void st7789_240_240_spotpear_initScreen(void)
{
    st7789_init(&lcd_config, MEMORY_WIDTH, MEMORY_HEIGHT);
    // Clear the 30-pixel letterbox margins before the first DOOM frame.
    st7789_fill(0x0000);
}

void st7789_240_240_spotpear_handleFrameStart(uint8_t frame)
{
    current_output_row = 0;
    // Updating every fourth frame keeps the borders responsive without adding
    // 33% more SPI traffic to every rendered frame.
    update_ambient_bands = (frame & AMBIENT_UPDATE_MASK) == 0;
    st7789_set_window(SPOTPEAR_X_OFFSET, SPOTPEAR_Y_OFFSET,
                      SPOTPEAR_OUTPUT_WIDTH, SPOTPEAR_OUTPUT_HEIGHT);
}

static void scale_horizontal(uint16_t *source)
{
    for (uint16_t x = 0; x < SPOTPEAR_OUTPUT_WIDTH; ++x) {
        const uint16_t source_x = (uint16_t)
            ((uint32_t)x * SCREENWIDTH / SPOTPEAR_OUTPUT_WIDTH);
        scaled_line[x] = source[source_x];
    }
}

static uint16_t dim_rgb565(uint16_t pixel, uint8_t strength)
{
    const uint16_t red = (uint16_t)
        (((uint32_t)((pixel >> 11) & 0x1f) * strength / 255) << 11);
    const uint16_t green = (uint16_t)
        (((uint32_t)((pixel >> 5) & 0x3f) * strength / 255) << 5);
    const uint16_t blue = (uint16_t)
        ((uint32_t)(pixel & 0x1f) * strength / 255);
    return red | green | blue;
}

static void blur_edge_row(void)
{
    for (uint16_t x = 0; x < SPOTPEAR_OUTPUT_WIDTH; ++x) {
        uint32_t red = 0;
        uint32_t green = 0;
        uint32_t blue = 0;
        uint16_t count = 0;
        const int first = (int)x - AMBIENT_BLUR_RADIUS;
        const int last = (int)x + AMBIENT_BLUR_RADIUS;

        for (int sample = first; sample <= last; ++sample) {
            const uint16_t source_x = (uint16_t)
                (sample < 0 ? 0
                 : sample >= SPOTPEAR_OUTPUT_WIDTH
                    ? SPOTPEAR_OUTPUT_WIDTH - 1
                    : sample);
            const uint16_t pixel = scaled_line[source_x];
            red += (pixel >> 11) & 0x1f;
            green += (pixel >> 5) & 0x3f;
            blue += pixel & 0x1f;
            ++count;
        }

        blurred_edge[x] = (uint16_t)(((red / count) << 11)
                            | ((green / count) << 5)
                            | (blue / count));
    }
}

static void write_ambient_band(uint16_t y, bool top)
{
    blur_edge_row();
    st7789_set_window(0, y, LCD_WIDTH, AMBIENT_BAND_HEIGHT);

    for (uint16_t row = 0; row < AMBIENT_BAND_HEIGHT; ++row) {
        const uint16_t distance = top
            ? AMBIENT_BAND_HEIGHT - 1 - row
            : row;
        // About 65% brightness at the scene boundary, fading to 12% at the
        // outer display edge.
        const uint8_t strength = (uint8_t)
            (166 - distance * 135 / (AMBIENT_BAND_HEIGHT - 1));
        for (uint16_t x = 0; x < SPOTPEAR_OUTPUT_WIDTH; ++x) {
            ambient_line[x] = dim_rgb565(blurred_edge[x], strength);
        }
        st7789_write(ambient_line, sizeof(ambient_line));
    }
}

void st7789_240_240_spotpear_handleScanline(uint16_t *line, int scanline)
{
    // Select 180 of DOOM's 200 source rows and scale each selected row from
    // 320 to 240 pixels. This is the game's intended 4:3 display geometry.
    if (current_output_row >= SPOTPEAR_OUTPUT_HEIGHT) {
        return;
    }

    const uint16_t source_y = (uint16_t)
        ((uint32_t)current_output_row * SCREENHEIGHT / SPOTPEAR_OUTPUT_HEIGHT);
    if ((uint16_t)scanline != source_y) {
        return;
    }

    scale_horizontal(line);

    if (update_ambient_bands && current_output_row == 0) {
        write_ambient_band(0, true);
        // The ambient band changes the controller window, so restore the
        // main frame window before streaming the first scene row.
        st7789_set_window(SPOTPEAR_X_OFFSET, SPOTPEAR_Y_OFFSET,
                          SPOTPEAR_OUTPUT_WIDTH, SPOTPEAR_OUTPUT_HEIGHT);
    }

    st7789_write(scaled_line, sizeof(scaled_line));
    ++current_output_row;

    if (update_ambient_bands
        && current_output_row == SPOTPEAR_OUTPUT_HEIGHT) {
        write_ambient_band(SPOTPEAR_Y_OFFSET + SPOTPEAR_OUTPUT_HEIGHT, false);
    }
}
