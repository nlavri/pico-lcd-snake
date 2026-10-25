#include "platform/display.h"

#include <stddef.h>

static void write_rgb565(uint8_t *destination, DisplayColor color)
{
    destination[0] = (uint8_t)(color >> 8);
    destination[1] = (uint8_t)color;
}

void display_fill_rectangle(
    DisplaySurface *surface,
    int x,
    int y,
    int width,
    int height,
    DisplayColor color)
{
    if (surface == NULL || surface->pixels == NULL || width <= 0 || height <= 0)
    {
        return;
    }

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;
    int y1 = y + height;
    if (x1 > surface->width)
    {
        x1 = surface->width;
    }
    if (y1 > surface->height)
    {
        y1 = surface->height;
    }
    if (x0 >= x1 || y0 >= y1)
    {
        return;
    }

    uint8_t *bytes = (uint8_t *)surface->pixels;
    for (int target_y = y0; target_y < y1; target_y++)
    {
        uint8_t *destination = bytes +
            (((size_t)target_y * surface->width + (size_t)x0) * 2u);
        for (int target_x = x0; target_x < x1; target_x++)
        {
            write_rgb565(destination, color);
            destination += 2;
        }
    }
}

void display_blit_indexed(
    DisplaySurface *surface,
    int x,
    int y,
    const uint8_t *pixels,
    uint16_t width,
    uint16_t height,
    const DisplayColor *palette,
    uint8_t palette_size,
    uint8_t transparent_index,
    bool flip_x)
{
    display_blit_indexed_scaled(
        surface,
        x,
        y,
        pixels,
        width,
        height,
        palette,
        palette_size,
        transparent_index,
        flip_x,
        1);
}

void display_blit_indexed_scaled(
    DisplaySurface *surface,
    int x,
    int y,
    const uint8_t *pixels,
    uint16_t width,
    uint16_t height,
    const DisplayColor *palette,
    uint8_t palette_size,
    uint8_t transparent_index,
    bool flip_x,
    uint8_t scale)
{
    if (surface == NULL || surface->pixels == NULL || pixels == NULL ||
        palette == NULL || width == 0 || height == 0 || palette_size == 0 ||
        scale == 0)
    {
        return;
    }

    uint8_t *bytes = (uint8_t *)surface->pixels;
    const uint32_t output_width = (uint32_t)width * scale;
    const uint32_t output_height = (uint32_t)height * scale;
    for (uint32_t output_y = 0; output_y < output_height; output_y++)
    {
        const int target_y = y + (int)output_y;
        if (target_y < 0 || target_y >= surface->height)
        {
            continue;
        }

        const uint16_t source_y = (uint16_t)(output_y / scale);
        for (uint32_t output_x = 0; output_x < output_width; output_x++)
        {
            const int target_x = x + (int)output_x;
            if (target_x < 0 || target_x >= surface->width)
            {
                continue;
            }

            const uint16_t unflipped_x = (uint16_t)(output_x / scale);
            const uint16_t source_x = flip_x
                ? (uint16_t)(width - 1u - unflipped_x)
                : unflipped_x;
            const uint8_t index = pixels[(size_t)source_y * width + source_x];
            if (index == transparent_index || index >= palette_size)
            {
                continue;
            }

            write_rgb565(
                bytes + (((size_t)target_y * surface->width +
                    (size_t)target_x) * 2u),
                palette[index]);
        }
    }
}
