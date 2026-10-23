#include "temperature_app.h"

#include <stdio.h>

#define TEMPERATURE_UPDATE_INTERVAL_MS 500

typedef struct TemperatureAppState
{
    AppContext *context;
    float temperature_c;
    uint32_t elapsed_ms;
} TemperatureAppState;

typedef struct TemperatureColorStop
{
    float temperature_c;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} TemperatureColorStop;

static const TemperatureColorStop color_stops[] = {
    {-20.0f, 112, 0, 255},
    {0.0f, 0, 96, 255},
    {10.0f, 0, 224, 255},
    {20.0f, 0, 224, 80},
    {27.0f, 255, 224, 0},
    {37.0f, 255, 96, 0},
    {50.0f, 255, 0, 0},
};

static DisplayColor rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (DisplayColor)(
        ((uint16_t)(red & 0xF8) << 8) |
        ((uint16_t)(green & 0xFC) << 3) |
        (blue >> 3));
}

static uint8_t interpolate_channel(uint8_t from, uint8_t to, float position)
{
    return (uint8_t)(from + ((float)to - from) * position);
}

static DisplayColor temperature_color(float temperature_c)
{
    const size_t stop_count = sizeof(color_stops) / sizeof(color_stops[0]);
    if (temperature_c <= color_stops[0].temperature_c)
    {
        return rgb565(
            color_stops[0].red,
            color_stops[0].green,
            color_stops[0].blue);
    }

    for (size_t i = 1; i < stop_count; i++)
    {
        if (temperature_c <= color_stops[i].temperature_c)
        {
            const TemperatureColorStop *low = &color_stops[i - 1];
            const TemperatureColorStop *high = &color_stops[i];
            const float position =
                (temperature_c - low->temperature_c) /
                (high->temperature_c - low->temperature_c);
            return rgb565(
                interpolate_channel(low->red, high->red, position),
                interpolate_channel(low->green, high->green, position),
                interpolate_channel(low->blue, high->blue, position));
        }
    }

    const TemperatureColorStop *last = &color_stops[stop_count - 1];
    return rgb565(last->red, last->green, last->blue);
}

static const char *temperature_description(float temperature_c)
{
    if (temperature_c < 0.0f)
    {
        return "FREEZING";
    }
    if (temperature_c < 10.0f)
    {
        return "COLD";
    }
    if (temperature_c < 20.0f)
    {
        return "COOL";
    }
    if (temperature_c < 30.0f)
    {
        return "WARM";
    }
    if (temperature_c < 40.0f)
    {
        return "HOT";
    }
    return "VERY HOT";
}

static void read_temperature(TemperatureAppState *state)
{
    state->temperature_c = state->context->read_temperature_c();
}

static bool temperature_start(void *state_memory, AppContext *context)
{
    if (context->read_temperature_c == NULL)
    {
        return false;
    }

    TemperatureAppState *state = state_memory;
    state->context = context;
    read_temperature(state);
    return true;
}

static void temperature_resume(void *state_memory)
{
    TemperatureAppState *state = state_memory;
    state->elapsed_ms = 0;
    read_temperature(state);
}

static AppResult temperature_update(void *state_memory, uint32_t elapsed_ms)
{
    TemperatureAppState *state = state_memory;
    state->elapsed_ms += elapsed_ms;
    if (state->elapsed_ms < TEMPERATURE_UPDATE_INTERVAL_MS)
    {
        return APP_RESULT_CONTINUE;
    }

    state->elapsed_ms %= TEMPERATURE_UPDATE_INTERVAL_MS;
    read_temperature(state);
    return APP_RESULT_REDRAW;
}

static uint16_t centered_x(
    const DisplaySurface *surface,
    const char *text,
    DisplayFont font)
{
    const uint16_t width = display_text_width(text, font);
    return width >= surface->width ? 0 : (surface->width - width) / 2;
}

static void temperature_render(
    const void *state_memory,
    DisplaySurface *surface)
{
    const TemperatureAppState *state = state_memory;
    const DisplayColor color = temperature_color(state->temperature_c);
    char value[16];
    snprintf(value, sizeof(value), "%.1f C", state->temperature_c);

    display_clear(surface, DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface,
        centered_x(surface, "TEMPERATURE", DISPLAY_FONT_MEDIUM),
        8,
        "TEMPERATURE",
        DISPLAY_FONT_MEDIUM,
        DISPLAY_COLOR_WHITE,
        DISPLAY_COLOR_BLACK);
    display_draw_rectangle(
        surface, 12, 38, surface->width - 13, 89, color, 2);
    display_draw_text_with_font(
        surface,
        centered_x(surface, value, DISPLAY_FONT_LARGE),
        50,
        value,
        DISPLAY_FONT_LARGE,
        color,
        DISPLAY_COLOR_BLACK);

    const char *description = temperature_description(state->temperature_c);
    display_draw_text(
        surface,
        centered_x(surface, description, DISPLAY_FONT_SMALL),
        101,
        description,
        color,
        DISPLAY_COLOR_BLACK);
    display_draw_text(
        surface,
        centered_x(surface, "RP2040 INTERNAL SENSOR", DISPLAY_FONT_SMALL),
        surface->height - display_font_height(DISPLAY_FONT_SMALL) - 2,
        "RP2040 INTERNAL SENSOR",
        DISPLAY_COLOR_GRAY,
        DISPLAY_COLOR_BLACK);
}

const Application temperature_application = {
    .name = "Temperature",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_TEMPERATURE,
    .state_size = sizeof(TemperatureAppState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = temperature_start,
    .pause = NULL,
    .resume = temperature_resume,
    .stop = NULL,
    .handle_event = NULL,
    .update = temperature_update,
    .render = temperature_render,
};
