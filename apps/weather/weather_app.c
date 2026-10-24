#include "weather_app.h"

#include <stdio.h>
#include <string.h>

#include "platform/network.h"
#include "platform/weather.h"
#include "ui.h"

#define WEATHER_REFRESH_INTERVAL_MS 200
#define WEATHER_PAGE_COUNT 3
typedef enum WeatherScreen
{
    WEATHER_SCREEN_CONTENT,
    WEATHER_SCREEN_SETTINGS,
    WEATHER_SCREEN_THEMES,
} WeatherScreen;

typedef enum WeatherTheme
{
    WEATHER_THEME_ASCII,
    WEATHER_THEME_GRAPHICS,
    WEATHER_THEME_COUNT,
} WeatherTheme;

typedef struct WeatherAppState
{
    AppContext *context;
    NetworkSnapshot network;
    WeatherSnapshot weather;
    uint32_t refresh_elapsed_ms;
    bool refresh_pending;
    uint8_t page;
    WeatherScreen screen;
    WeatherTheme theme;
    WeatherTheme selected_theme;
} WeatherAppState;

static uint16_t centered_x(
    const DisplaySurface *surface,
    const char *text,
    DisplayFont font)
{
    const uint16_t width = display_text_width(text, font);
    return width >= surface->width ? 0 : (surface->width - width) / 2;
}

static void refresh_snapshots(WeatherAppState *state)
{
    state->context->network->get_snapshot(&state->network);
    state->context->weather->get_snapshot(&state->weather);
}

static bool request_weather_if_connected(WeatherAppState *state)
{
    refresh_snapshots(state);
    if (state->network.status == NETWORK_STATUS_CONNECTED &&
        state->weather.status != WEATHER_STATUS_LOADING)
    {
        state->refresh_pending =
            state->context->weather->request_refresh();
        if (state->refresh_pending)
        {
            state->context->weather->get_snapshot(&state->weather);
        }
        return state->refresh_pending;
    }
    return false;
}

static bool weather_start(void *state_memory, AppContext *context)
{
    if (context->network == NULL || context->weather == NULL)
    {
        return false;
    }

    WeatherAppState *state = state_memory;
    state->context = context;
    state->page = 0;
    state->screen = WEATHER_SCREEN_CONTENT;
    state->theme = WEATHER_THEME_ASCII;
    state->selected_theme = state->theme;
    request_weather_if_connected(state);
    return true;
}

static void weather_resume(void *state_memory)
{
    WeatherAppState *state = state_memory;
    state->refresh_elapsed_ms = 0;
    state->screen = WEATHER_SCREEN_CONTENT;
    refresh_snapshots(state);
}

static const char *weather_theme_name(WeatherTheme theme)
{
    return theme == WEATHER_THEME_GRAPHICS ? "Graphics" : "ASCII";
}

static AppResult handle_content_event(
    WeatherAppState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_B)
    {
        return APP_RESULT_HOME;
    }
    if (event->button == INPUT_CLICK)
    {
        state->screen = WEATHER_SCREEN_SETTINGS;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT)
    {
        state->page = state->page == 0
            ? WEATHER_PAGE_COUNT - 1
            : state->page - 1;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_RIGHT)
    {
        state->page = (state->page + 1) % WEATHER_PAGE_COUNT;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_A)
    {
        request_weather_if_connected(state);
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_settings_event(
    WeatherAppState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        state->selected_theme = state->theme;
        state->screen = WEATHER_SCREEN_THEMES;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        state->screen = WEATHER_SCREEN_CONTENT;
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_themes_event(
    WeatherAppState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_UP || event->button == INPUT_DOWN)
    {
        state->selected_theme = state->selected_theme == WEATHER_THEME_ASCII
            ? WEATHER_THEME_GRAPHICS
            : WEATHER_THEME_ASCII;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        state->theme = state->selected_theme;
        state->screen = WEATHER_SCREEN_SETTINGS;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        state->screen = WEATHER_SCREEN_SETTINGS;
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult weather_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    WeatherAppState *state = state_memory;
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    switch (state->screen)
    {
        case WEATHER_SCREEN_CONTENT:
            return event->action == INPUT_PRESSED
                ? handle_content_event(state, event)
                : APP_RESULT_CONTINUE;
        case WEATHER_SCREEN_SETTINGS:
            return event->action == INPUT_PRESSED
                ? handle_settings_event(state, event)
                : APP_RESULT_CONTINUE;
        case WEATHER_SCREEN_THEMES:
            return handle_themes_event(state, event);
    }
    return APP_RESULT_CONTINUE;
}

static AppResult weather_update(void *state_memory, uint32_t elapsed_ms)
{
    WeatherAppState *state = state_memory;
    state->refresh_elapsed_ms += elapsed_ms;
    if (state->refresh_elapsed_ms < WEATHER_REFRESH_INTERVAL_MS)
    {
        return APP_RESULT_CONTINUE;
    }
    state->refresh_elapsed_ms %= WEATHER_REFRESH_INTERVAL_MS;

    const uint32_t network_revision = state->network.revision;
    const uint32_t weather_revision = state->weather.revision;
    refresh_snapshots(state);
    if (state->weather.status != WEATHER_STATUS_IDLE &&
        state->weather.status != WEATHER_STATUS_UNAVAILABLE)
    {
        state->refresh_pending = false;
    }
    if (!state->refresh_pending &&
        state->network.status == NETWORK_STATUS_CONNECTED &&
        (state->weather.status == WEATHER_STATUS_IDLE ||
         state->weather.status == WEATHER_STATUS_UNAVAILABLE ||
         state->weather.status == WEATHER_STATUS_OFFLINE))
    {
        request_weather_if_connected(state);
    }
    return network_revision != state->network.revision ||
            weather_revision != state->weather.revision
        ? APP_RESULT_REDRAW
        : APP_RESULT_CONTINUE;
}

typedef struct WeatherTemperatureColorStop
{
    float temperature_c;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} WeatherTemperatureColorStop;

static const WeatherTemperatureColorStop weather_temperature_color_stops[] = {
    {-20.0f, 112, 0, 255},
    {0.0f, 0, 96, 255},
    {10.0f, 0, 224, 255},
    {20.0f, 0, 224, 80},
    {27.0f, 255, 224, 0},
    {37.0f, 255, 96, 0},
    {50.0f, 255, 0, 0},
};

static DisplayColor weather_rgb565(
    uint8_t red,
    uint8_t green,
    uint8_t blue)
{
    return (DisplayColor)(
        ((uint16_t)(red & 0xF8) << 8) |
        ((uint16_t)(green & 0xFC) << 3) |
        (blue >> 3));
}

static uint8_t weather_interpolate_channel(
    uint8_t from,
    uint8_t to,
    float position)
{
    return (uint8_t)(from + ((float)to - from) * position);
}

static DisplayColor weather_temperature_color(float temperature_c)
{
    const size_t stop_count = sizeof(weather_temperature_color_stops) /
        sizeof(weather_temperature_color_stops[0]);
    if (temperature_c <= weather_temperature_color_stops[0].temperature_c)
    {
        const WeatherTemperatureColorStop *stop =
            &weather_temperature_color_stops[0];
        return weather_rgb565(stop->red, stop->green, stop->blue);
    }

    for (size_t i = 1; i < stop_count; i++)
    {
        if (temperature_c <= weather_temperature_color_stops[i].temperature_c)
        {
            const WeatherTemperatureColorStop *low =
                &weather_temperature_color_stops[i - 1];
            const WeatherTemperatureColorStop *high =
                &weather_temperature_color_stops[i];
            const float position =
                (temperature_c - low->temperature_c) /
                (high->temperature_c - low->temperature_c);
            return weather_rgb565(
                weather_interpolate_channel(low->red, high->red, position),
                weather_interpolate_channel(low->green, high->green, position),
                weather_interpolate_channel(low->blue, high->blue, position));
        }
    }

    const WeatherTemperatureColorStop *last =
        &weather_temperature_color_stops[stop_count - 1];
    return weather_rgb565(last->red, last->green, last->blue);
}

typedef enum WeatherIcon
{
    WEATHER_ICON_CLEAR,
    WEATHER_ICON_PARTLY_CLOUDY,
    WEATHER_ICON_CLOUDY,
    WEATHER_ICON_RAIN,
    WEATHER_ICON_SNOW,
    WEATHER_ICON_FOG,
    WEATHER_ICON_THUNDER,
} WeatherIcon;

static WeatherIcon weather_icon_for_code(uint16_t code)
{
    if (code == 113)
    {
        return WEATHER_ICON_CLEAR;
    }
    if (code == 116)
    {
        return WEATHER_ICON_PARTLY_CLOUDY;
    }
    if (code == 143 || code == 248 || code == 260)
    {
        return WEATHER_ICON_FOG;
    }
    if (code == 200 || code == 386 || code == 389 ||
        code == 392 || code == 395)
    {
        return WEATHER_ICON_THUNDER;
    }
    switch (code)
    {
        case 179:
        case 182:
        case 185:
        case 227:
        case 230:
        case 281:
        case 284:
        case 311:
        case 314:
        case 317:
        case 320:
        case 323:
        case 326:
        case 329:
        case 332:
        case 335:
        case 338:
        case 350:
        case 362:
        case 365:
        case 368:
        case 371:
        case 374:
        case 377:
            return WEATHER_ICON_SNOW;
        case 176:
        case 263:
        case 266:
        case 293:
        case 296:
        case 299:
        case 302:
        case 305:
        case 308:
        case 353:
        case 356:
        case 359:
            return WEATHER_ICON_RAIN;
        default:
            return WEATHER_ICON_CLOUDY;
    }
}

static void draw_filled_rectangle(
    DisplaySurface *surface,
    int x,
    int y,
    int width,
    int height,
    DisplayColor color)
{
    for (int row = 0; row < height; row++)
    {
        const int target_y = y + row;
        if (target_y < 0 || target_y >= surface->height)
        {
            continue;
        }
        for (int column = 0; column < width; column++)
        {
            const int target_x = x + column;
            if (target_x >= 0 && target_x < surface->width)
            {
                display_draw_point(
                    surface,
                    (uint16_t)target_x,
                    (uint16_t)target_y,
                    color,
                    1);
            }
        }
    }
}

static void draw_filled_circle(
    DisplaySurface *surface,
    int center_x,
    int center_y,
    int radius,
    DisplayColor color)
{
    const int radius_squared = radius * radius;
    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            if (x * x + y * y <= radius_squared)
            {
                draw_filled_rectangle(
                    surface,
                    center_x + x,
                    center_y + y,
                    1,
                    1,
                    color);
            }
        }
    }
}

static void draw_graphic_line(
    DisplaySurface *surface,
    int x0,
    int y0,
    int x1,
    int y1,
    DisplayColor color)
{
    const int delta_x = x1 >= x0 ? x1 - x0 : x0 - x1;
    const int step_x = x0 < x1 ? 1 : -1;
    const int delta_y = -(y1 >= y0 ? y1 - y0 : y0 - y1);
    const int step_y = y0 < y1 ? 1 : -1;
    int error = delta_x + delta_y;

    for (;;)
    {
        draw_filled_rectangle(surface, x0, y0, 2, 2, color);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        const int twice_error = error * 2;
        if (twice_error >= delta_y)
        {
            error += delta_y;
            x0 += step_x;
        }
        if (twice_error <= delta_x)
        {
            error += delta_x;
            y0 += step_y;
        }
    }
}

typedef struct WindDirectionVector
{
    const char *name;
    int8_t x;
    int8_t y;
} WindDirectionVector;

static bool wind_direction_vector(
    const char *direction,
    int *x,
    int *y)
{
    static const WindDirectionVector directions[] = {
        {"N", 0, -6}, {"NNE", 2, -6}, {"NE", 4, -4},
        {"ENE", 6, -2}, {"E", 6, 0}, {"ESE", 6, 2},
        {"SE", 4, 4}, {"SSE", 2, 6}, {"S", 0, 6},
        {"SSW", -2, 6}, {"SW", -4, 4}, {"WSW", -6, 2},
        {"W", -6, 0}, {"WNW", -6, -2}, {"NW", -4, -4},
        {"NNW", -2, -6},
    };
    for (size_t i = 0; i < sizeof(directions) / sizeof(directions[0]); i++)
    {
        if (strcmp(direction, directions[i].name) == 0)
        {
            *x = directions[i].x;
            *y = directions[i].y;
            return true;
        }
    }
    return false;
}

static bool draw_wind_arrow(
    DisplaySurface *surface,
    int center_x,
    int center_y,
    const char *direction)
{
    int vector_x;
    int vector_y;
    if (!wind_direction_vector(direction, &vector_x, &vector_y))
    {
        return false;
    }

    const DisplayColor color = ui_accent_color();
    const int end_x = center_x + vector_x;
    const int end_y = center_y + vector_y;
    const int back_x = end_x - vector_x / 2;
    const int back_y = end_y - vector_y / 2;
    const int perpendicular_x = -vector_y / 3;
    const int perpendicular_y = vector_x / 3;
    draw_graphic_line(
        surface,
        center_x - vector_x,
        center_y - vector_y,
        end_x,
        end_y,
        color);
    draw_graphic_line(
        surface,
        end_x,
        end_y,
        back_x + perpendicular_x,
        back_y + perpendicular_y,
        color);
    draw_graphic_line(
        surface,
        end_x,
        end_y,
        back_x - perpendicular_x,
        back_y - perpendicular_y,
        color);
    return true;
}

static void draw_wind_row(
    DisplaySurface *surface,
    WeatherTheme theme,
    uint16_t x,
    uint16_t y,
    const char *direction,
    uint16_t speed_kmph)
{
    char text[24];
    if (theme == WEATHER_THEME_GRAPHICS &&
        draw_wind_arrow(surface, x + 7, y + 6, direction))
    {
        snprintf(text, sizeof(text), "%s %u km/h", direction, speed_kmph);
        display_draw_text(
            surface, x + 20, y, text,
            ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
        return;
    }

    if (direction[0] == '\0')
    {
        snprintf(text, sizeof(text), "Wind %u km/h", speed_kmph);
    }
    else
    {
        snprintf(text, sizeof(text), "Wind %s %u km/h", direction, speed_kmph);
    }
    display_draw_text(
        surface, x, y, text,
        ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
}

static void draw_graphic_sun(
    DisplaySurface *surface,
    int x,
    int y,
    int scale)
{
    const DisplayColor yellow = weather_rgb565(255, 224, 75);
    const DisplayColor orange = weather_rgb565(255, 159, 28);
    draw_filled_rectangle(surface, x + 30 * scale, y + 9 * scale,
                          4 * scale, 7 * scale, yellow);
    draw_filled_rectangle(surface, x + 30 * scale, y + 32 * scale,
                          4 * scale, 7 * scale, yellow);
    draw_filled_rectangle(surface, x + 17 * scale, y + 22 * scale,
                          7 * scale, 4 * scale, yellow);
    draw_filled_rectangle(surface, x + 40 * scale, y + 22 * scale,
                          7 * scale, 4 * scale, yellow);
    draw_filled_rectangle(surface, x + 21 * scale, y + 13 * scale,
                          5 * scale, 4 * scale, yellow);
    draw_filled_rectangle(surface, x + 38 * scale, y + 13 * scale,
                          5 * scale, 4 * scale, yellow);
    draw_filled_rectangle(surface, x + 21 * scale, y + 31 * scale,
                          5 * scale, 4 * scale, yellow);
    draw_filled_rectangle(surface, x + 38 * scale, y + 31 * scale,
                          5 * scale, 4 * scale, yellow);
    draw_filled_circle(surface, x + 32 * scale, y + 24 * scale,
                       9 * scale, orange);
    draw_filled_circle(surface, x + 32 * scale, y + 24 * scale,
                       6 * scale, yellow);
}

static void draw_graphic_cloud(
    DisplaySurface *surface,
    int x,
    int y,
    DisplayColor front,
    DisplayColor back,
    int scale)
{
    draw_filled_circle(surface, x + 18 * scale, y + 22 * scale,
                       10 * scale, back);
    draw_filled_circle(surface, x + 31 * scale, y + 16 * scale,
                       14 * scale, back);
    draw_filled_circle(surface, x + 45 * scale, y + 22 * scale,
                       11 * scale, back);
    draw_filled_rectangle(surface, x + 16 * scale, y + 20 * scale,
                          33 * scale, 15 * scale, back);
    draw_filled_circle(surface, x + 17 * scale, y + 20 * scale,
                       9 * scale, front);
    draw_filled_circle(surface, x + 30 * scale, y + 14 * scale,
                       12 * scale, front);
    draw_filled_circle(surface, x + 44 * scale, y + 21 * scale,
                       9 * scale, front);
    draw_filled_rectangle(surface, x + 15 * scale, y + 20 * scale,
                          34 * scale, 11 * scale, front);
}

static void draw_graphic_rain(
    DisplaySurface *surface,
    int x,
    int y,
    int scale)
{
    const DisplayColor cyan = weather_rgb565(32, 221, 255);
    const int drops[] = {17, 29, 41};
    for (size_t i = 0; i < sizeof(drops) / sizeof(drops[0]); i++)
    {
        draw_filled_rectangle(surface, x + drops[i] * scale, y + 31 * scale,
                              3 * scale, 7 * scale, cyan);
        draw_filled_rectangle(surface, x + (drops[i] - 2) * scale,
                              y + 37 * scale, 3 * scale, 4 * scale, cyan);
    }
}

static void draw_graphic_snow(
    DisplaySurface *surface,
    int x,
    int y,
    int scale)
{
    const int flakes[] = {18, 31, 44};
    for (size_t i = 0; i < sizeof(flakes) / sizeof(flakes[0]); i++)
    {
        draw_filled_rectangle(
            surface, x + (flakes[i] - 1) * scale, y + 32 * scale,
            3 * scale, 9 * scale, DISPLAY_COLOR_WHITE);
        draw_filled_rectangle(
            surface, x + (flakes[i] - 4) * scale, y + 35 * scale,
            9 * scale, 3 * scale, DISPLAY_COLOR_WHITE);
    }
}

static void draw_graphic_lightning(
    DisplaySurface *surface,
    int x,
    int y,
    int scale)
{
    const DisplayColor violet = weather_rgb565(168, 85, 247);
    draw_filled_rectangle(surface, x + 31 * scale, y + 26 * scale,
                          8 * scale, 4 * scale, violet);
    draw_filled_rectangle(surface, x + 28 * scale, y + 30 * scale,
                          8 * scale, 4 * scale, violet);
    draw_filled_rectangle(surface, x + 25 * scale, y + 34 * scale,
                          8 * scale, 4 * scale, violet);
    draw_filled_rectangle(surface, x + 27 * scale, y + 38 * scale,
                          5 * scale, 3 * scale, violet);
    draw_filled_rectangle(surface, x + 24 * scale, y + 41 * scale,
                          5 * scale, 4 * scale, violet);
}

static void draw_graphic_weather(
    DisplaySurface *surface,
    int x,
    int y,
    uint16_t code,
    int scale)
{
    const WeatherIcon icon = weather_icon_for_code(code);
    const DisplayColor gray_dark = weather_rgb565(63, 74, 82);
    const DisplayColor gray_mid = weather_rgb565(116, 128, 137);
    const DisplayColor gray_light = weather_rgb565(183, 192, 197);
    if (icon == WEATHER_ICON_CLEAR)
    {
        draw_graphic_sun(surface, x, y, scale);
        return;
    }
    if (icon == WEATHER_ICON_PARTLY_CLOUDY)
    {
        draw_graphic_sun(surface, x - 11 * scale, y - 8 * scale, scale);
        draw_graphic_cloud(
            surface, x + 7 * scale, y + 7 * scale,
            gray_light, gray_dark, scale);
        return;
    }
    if (icon == WEATHER_ICON_FOG)
    {
        draw_graphic_cloud(
            surface, x + 2 * scale, y - 2 * scale,
            gray_mid, gray_dark, scale);
        draw_filled_rectangle(surface, x + 7 * scale, y + 31 * scale,
                              48 * scale, 3 * scale, gray_light);
        draw_filled_rectangle(surface, x + 13 * scale, y + 38 * scale,
                              42 * scale, 3 * scale, gray_light);
        draw_filled_rectangle(surface, x + 7 * scale, y + 45 * scale,
                              48 * scale, 3 * scale, gray_light);
        return;
    }
    if (icon == WEATHER_ICON_THUNDER)
    {
        const DisplayColor electric_blue = weather_rgb565(22, 140, 255);
        const DisplayColor blue_shadow = weather_rgb565(18, 62, 118);
        draw_graphic_cloud(
            surface, x + 2 * scale, y + 1 * scale,
            electric_blue, blue_shadow, scale);
        draw_graphic_lightning(surface, x, y, scale);
        return;
    }
    if (icon == WEATHER_ICON_SNOW)
    {
        draw_graphic_cloud(
            surface, x + 2 * scale, y + 1 * scale,
            gray_light, gray_dark, scale);
        draw_graphic_snow(surface, x, y, scale);
        return;
    }
    if (icon == WEATHER_ICON_RAIN)
    {
        draw_graphic_cloud(
            surface, x + 2 * scale, y + 1 * scale,
            gray_mid, gray_dark, scale);
        draw_graphic_rain(surface, x, y, scale);
        return;
    }
    draw_graphic_cloud(
        surface, x + 2 * scale, y + 6 * scale,
        gray_light, gray_dark, scale);
}

typedef struct WeatherAsciiGlyph
{
    const char *lines[5];
    DisplayColor colors[5];
} WeatherAsciiGlyph;

static WeatherAsciiGlyph weather_ascii_glyph(uint16_t code)
{
    const DisplayColor yellow = weather_rgb565(255, 224, 0);
    const DisplayColor cloud = weather_rgb565(198, 195, 198);
    const DisplayColor cloud_dark = weather_rgb565(104, 116, 124);
    const DisplayColor cyan = weather_rgb565(33, 223, 255);
    const DisplayColor fog = weather_rgb565(169, 173, 175);
    const DisplayColor blue = weather_rgb565(22, 140, 255);
    const DisplayColor violet = weather_rgb565(168, 85, 247);
    const WeatherIcon icon = weather_icon_for_code(code);
    switch (icon)
    {
        case WEATHER_ICON_CLEAR:
            return (WeatherAsciiGlyph){
                .lines = {
                    "    \\ | /", "     .-.", "  --(   )--",
                    "     `-'", "    / | \\",
                },
                .colors = {yellow, yellow, yellow, yellow, yellow},
            };
        case WEATHER_ICON_PARTLY_CLOUDY:
            return (WeatherAsciiGlyph){
                .lines = {
                    "     \\|/", "   .-.-.", ".-(     ).",
                    "(___.__)__)", "",
                },
                .colors = {yellow, yellow, cloud, cloud, cloud},
            };
        case WEATHER_ICON_RAIN:
            return (WeatherAsciiGlyph){
                .lines = {
                    "     .--.", "  .-(    ).", " (___.__)__)",
                    "    /  /  /", "   /  /  /",
                },
                .colors = {cloud_dark, cloud_dark, cloud_dark, cyan, cyan},
            };
        case WEATHER_ICON_SNOW:
            return (WeatherAsciiGlyph){
                .lines = {
                    "     .--.", "  .-(    ).", " (___.__)__)",
                    "    *  *  *", "  *  *  *",
                },
                .colors = {cloud, cloud, cloud, DISPLAY_COLOR_WHITE,
                           DISPLAY_COLOR_WHITE},
            };
        case WEATHER_ICON_FOG:
            return (WeatherAsciiGlyph){
                .lines = {
                    "  ~~~~~~~~~~", " ~~~~~~~~~~", "  ~~~~~~~~~~",
                    " ~~~~~~~~~~", "  ~~~~~~~~~~",
                },
                .colors = {fog, fog, fog, fog, fog},
            };
        case WEATHER_ICON_THUNDER:
            return (WeatherAsciiGlyph){
                .lines = {
                    "     .--.", "  .-(    ).", " (___.__)__)",
                    "      /_", "     /",
                },
                .colors = {blue, blue, blue, violet, violet},
            };
        case WEATHER_ICON_CLOUDY:
        default:
            return (WeatherAsciiGlyph){
                .lines = {
                    "     .--.", "  .-(    ).", " (___.__)__)",
                    "", "",
                },
                .colors = {cloud, cloud, cloud, cloud, cloud},
            };
    }
}

static void draw_ascii_weather(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    uint16_t code)
{
    const WeatherAsciiGlyph glyph = weather_ascii_glyph(code);
    const uint16_t line_height = display_font_height(DISPLAY_FONT_SMALL);
    for (size_t i = 0; i < sizeof(glyph.lines) / sizeof(glyph.lines[0]); i++)
    {
        display_draw_text(
            surface,
            x,
            y + (uint16_t)i * line_height,
            glyph.lines[i],
            glyph.colors[i],
            DISPLAY_COLOR_BLACK);
    }
}

static void draw_themed_weather(
    DisplaySurface *surface,
    WeatherTheme theme,
    uint16_t x,
    uint16_t y,
    uint16_t code)
{
    if (theme == WEATHER_THEME_GRAPHICS)
    {
        draw_graphic_weather(surface, x, y, code, 1);
    }
    else
    {
        draw_ascii_weather(surface, x, y, code);
    }
}

static void compact_condition(
    const char *condition,
    char output[16]);

static bool use_expanded_layout(const DisplaySurface *surface)
{
    return surface->height >= 200;
}

static void draw_ascii_weather_expanded(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t code)
{
    const WeatherAsciiGlyph glyph = weather_ascii_glyph(code);
    const DisplayFont font = DISPLAY_FONT_COMPACT;
    const uint16_t line_height = display_font_height(font);
    for (size_t i = 0; i < sizeof(glyph.lines) / sizeof(glyph.lines[0]); i++)
    {
        const uint16_t line_width = display_text_width(glyph.lines[i], font);
        const uint16_t line_x = line_width < width
            ? (uint16_t)(x + (width - line_width) / 2)
            : x;
        display_draw_text_with_font(
            surface,
            line_x,
            y + (uint16_t)i * line_height,
            glyph.lines[i],
            font,
            glyph.colors[i],
            DISPLAY_COLOR_BLACK);
    }
}

static void draw_themed_weather_expanded(
    DisplaySurface *surface,
    WeatherTheme theme,
    uint16_t code)
{
    if (theme == WEATHER_THEME_GRAPHICS)
    {
        draw_graphic_weather(surface, 4, 43, code, 2);
    }
    else
    {
        draw_ascii_weather_expanded(surface, 4, 43, 132, code);
    }
}

enum
{
    EXPANDED_CONDITION_LINE_CAPACITY = 15,
};

typedef struct ExpandedCondition
{
    char first[EXPANDED_CONDITION_LINE_CAPACITY];
    char second[EXPANDED_CONDITION_LINE_CAPACITY];
} ExpandedCondition;

static ExpandedCondition expanded_condition(const char *condition)
{
    ExpandedCondition result = {0};
    const size_t maximum = EXPANDED_CONDITION_LINE_CAPACITY - 1;
    const size_t length = strlen(condition);
    if (length <= maximum)
    {
        snprintf(result.first, sizeof(result.first), "%s", condition);
        return result;
    }

    size_t split = maximum;
    while (split > 0 && condition[split] != ' ')
    {
        split--;
    }
    if (split == 0)
    {
        split = maximum;
    }
    snprintf(result.first, sizeof(result.first), "%.*s", (int)split, condition);

    const char *remainder = condition + split;
    while (*remainder == ' ')
    {
        remainder++;
    }
    snprintf(result.second, sizeof(result.second), "%.*s",
             (int)maximum, remainder);
    return result;
}

static void draw_expanded_condition(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    const char *condition)
{
    const ExpandedCondition lines = expanded_condition(condition);
    display_draw_text(
        surface, x, y, lines.first,
        ui_neutral_text_color(), DISPLAY_COLOR_BLACK);
    if (lines.second[0] != '\0')
    {
        display_draw_text(
            surface, x, y + display_font_height(DISPLAY_FONT_SMALL),
            lines.second,
            ui_neutral_text_color(), DISPLAY_COLOR_BLACK);
    }
}

static void render_current_expanded(
    const WeatherAppState *state,
    DisplaySurface *surface)
{
    char location[34];
    char temperature[16];
    char feels[16];
    char humidity[16];
    char visibility[24];
    snprintf(location, sizeof(location), "%.33s", state->weather.location);
    snprintf(temperature, sizeof(temperature), "%d C",
             state->weather.temperature_c);
    snprintf(feels, sizeof(feels), "%d C", state->weather.feels_like_c);
    snprintf(humidity, sizeof(humidity), "%u%%",
             state->weather.humidity_percent);
    snprintf(visibility, sizeof(visibility), "Visibility  %u km",
             state->weather.visibility_km);

    display_draw_text(
        surface, 8, 22, location,
        ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
    draw_themed_weather_expanded(
        surface, state->theme, state->weather.condition_code);

    const uint16_t data_x = 140;
    draw_expanded_condition(
        surface, data_x, 43, state->weather.condition);
    display_draw_text_with_font(
        surface, data_x, 72, temperature, DISPLAY_FONT_LARGE,
        weather_temperature_color(state->weather.temperature_c),
        DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 106, "FEELS", DISPLAY_FONT_SMALL,
        ui_chrome_color(), DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 120, feels, DISPLAY_FONT_COMPACT,
        weather_temperature_color(state->weather.feels_like_c),
        DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 143, "HUMIDITY", DISPLAY_FONT_SMALL,
        ui_chrome_color(), DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 157, humidity, DISPLAY_FONT_COMPACT,
        ui_secondary_text_color(), DISPLAY_COLOR_BLACK);

    draw_wind_row(
        surface, state->theme, 12, 184,
        state->weather.wind_direction,
        state->weather.wind_speed_kmph);
    display_draw_text(
        surface, 12, 205, visibility,
        ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
}

static void render_current(
    const WeatherAppState *state,
    DisplaySurface *surface)
{
    if (use_expanded_layout(surface))
    {
        render_current_expanded(state, surface);
        return;
    }

    char temperature[16];
    char condition[16];
    char details[28];
    char visibility[24];
    char location[34];
    snprintf(location, sizeof(location), "%.33s", state->weather.location);
    snprintf(
        temperature,
        sizeof(temperature),
        "%d C",
        state->weather.temperature_c);
    snprintf(
        details,
        sizeof(details),
        "Feels %dC  Hum %u%%",
        state->weather.feels_like_c,
        state->weather.humidity_percent);
    snprintf(
        visibility,
        sizeof(visibility),
        "Visibility %u km",
        state->weather.visibility_km);
    compact_condition(state->weather.condition, condition);

    const DisplayColor color =
        weather_temperature_color(state->weather.temperature_c);
    display_draw_text_with_font(
        surface,
        8,
        21,
        location,
        DISPLAY_FONT_SMALL,
        ui_secondary_text_color(),
        DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface,
        112,
        48,
        temperature,
        DISPLAY_FONT_LARGE,
        color,
        DISPLAY_COLOR_BLACK);
    draw_themed_weather(
        surface, state->theme, 5, 35, state->weather.condition_code);
    display_draw_text(
        surface,
        112,
        35,
        condition,
        ui_neutral_text_color(),
        DISPLAY_COLOR_BLACK);
    const DisplayFont details_font =
        display_text_width(details, DISPLAY_FONT_SMALL) <=
                surface->width - 112
            ? DISPLAY_FONT_SMALL
            : DISPLAY_FONT_TINY;
    display_draw_text_with_font(
        surface,
        112,
        75,
        details,
        details_font,
        ui_secondary_text_color(),
        DISPLAY_COLOR_BLACK);

    const uint16_t wind_y =
        state->theme == WEATHER_THEME_GRAPHICS ? 90 : 87;
    const uint16_t visibility_y =
        state->theme == WEATHER_THEME_GRAPHICS ? 108 : 99;
    draw_wind_row(
        surface,
        state->theme,
        112,
        wind_y,
        state->weather.wind_direction,
        state->weather.wind_speed_kmph);
    display_draw_text(surface, 112, visibility_y, visibility,
                      ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
    // Current precipitation is hidden until the upstream value proves useful.
}

static void compact_condition(
    const char *condition,
    char output[16])
{
    snprintf(output, 16, "%.15s", condition);
}

static void render_forecast_day(
    DisplaySurface *surface,
    WeatherTheme theme,
    const WeatherForecastDay *day,
    const char *heading)
{
    char date[6] = "--/--";
    char condition[16];
    char range[18];
    char rain[20];
    if (day->date[4] == '-' && day->date[7] == '-')
    {
        date[0] = day->date[8];
        date[1] = day->date[9];
        date[2] = '/';
        date[3] = day->date[5];
        date[4] = day->date[6];
    }
    compact_condition(day->condition, condition);
    snprintf(range, sizeof(range), "%d..%dC",
             day->minimum_c, day->maximum_c);
    snprintf(rain, sizeof(rain), "RAIN  %u%%",
             day->rain_chance_percent);

    char heading_and_date[24];
    snprintf(
        heading_and_date,
        sizeof(heading_and_date),
        "%s  %s",
        heading,
        date);

    display_draw_text_with_font(
        surface,
        8,
        21,
        heading_and_date,
        DISPLAY_FONT_SMALL,
        ui_secondary_text_color(),
        DISPLAY_COLOR_BLACK);

    draw_themed_weather(surface, theme, 5, 35, day->condition_code);

    display_draw_text(
        surface,
        112,
        35,
        condition,
        ui_neutral_text_color(),
        DISPLAY_COLOR_BLACK);

    const DisplayFont range_font =
        display_text_width(range, DISPLAY_FONT_MEDIUM) <= 123
            ? DISPLAY_FONT_MEDIUM
            : DISPLAY_FONT_SMALL;
    const float midpoint_c =
        ((float)day->minimum_c + (float)day->maximum_c) / 2.0f;
    const DisplayColor range_color = weather_temperature_color(midpoint_c);
    display_draw_text_with_font(
        surface,
        112,
        48,
        range,
        range_font,
        range_color,
        DISPLAY_COLOR_BLACK);

    display_draw_text(
        surface,
        112,
        73,
        rain,
        ui_secondary_text_color(),
        DISPLAY_COLOR_BLACK);
    draw_wind_row(
        surface,
        theme,
        112,
        89,
        day->wind_direction,
        day->wind_speed_kmph);
}

static void render_forecast_day_expanded(
    DisplaySurface *surface,
    WeatherTheme theme,
    const WeatherForecastDay *day,
    const char *heading)
{
    char date[6] = "--/--";
    char heading_and_date[24];
    char minimum[12];
    char maximum[12];
    char rain[20];
    if (day->date[4] == '-' && day->date[7] == '-')
    {
        date[0] = day->date[8];
        date[1] = day->date[9];
        date[2] = '/';
        date[3] = day->date[5];
        date[4] = day->date[6];
    }
    snprintf(heading_and_date, sizeof(heading_and_date),
             "%s  %s", heading, date);
    snprintf(minimum, sizeof(minimum), "%d C", day->minimum_c);
    snprintf(maximum, sizeof(maximum), "%d C", day->maximum_c);
    snprintf(rain, sizeof(rain), "Rain  %u%%", day->rain_chance_percent);

    display_draw_text(
        surface, 8, 22, heading_and_date,
        ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
    draw_themed_weather_expanded(surface, theme, day->condition_code);

    const uint16_t data_x = 140;
    draw_expanded_condition(surface, data_x, 43, day->condition);
    display_draw_text_with_font(
        surface, data_x, 74, "LOW", DISPLAY_FONT_SMALL,
        ui_chrome_color(), DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 88, minimum, DISPLAY_FONT_LARGE,
        weather_temperature_color(day->minimum_c), DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 122, "HIGH", DISPLAY_FONT_SMALL,
        ui_chrome_color(), DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, data_x, 136, maximum, DISPLAY_FONT_LARGE,
        weather_temperature_color(day->maximum_c), DISPLAY_COLOR_BLACK);

    display_draw_text(
        surface, 12, 177, rain,
        ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
    draw_wind_row(
        surface, theme, 12, 203,
        day->wind_direction, day->wind_speed_kmph);
}

static void render_forecast_page(
    const WeatherAppState *state,
    DisplaySurface *surface,
    uint8_t page)
{
    const uint8_t first_forecast =
        state->weather.forecast_count >= 3 ? 1 : 0;
    const uint8_t forecast_index = first_forecast + page - 1;
    if (forecast_index >= state->weather.forecast_count)
    {
        const char *message = "FORECAST UNAVAILABLE";
        display_draw_text(
            surface,
            centered_x(surface, message, DISPLAY_FONT_SMALL),
            65,
            message,
            ui_secondary_text_color(),
            DISPLAY_COLOR_BLACK);
        return;
    }
    if (use_expanded_layout(surface))
    {
        render_forecast_day_expanded(
            surface,
            state->theme,
            &state->weather.forecast[forecast_index],
            page == 1 ? "TOMORROW" : "+2 DAYS");
    }
    else
    {
        render_forecast_day(
            surface,
            state->theme,
            &state->weather.forecast[forecast_index],
            page == 1 ? "TOMORROW" : "+2 DAYS");
    }
}

static void render_weather_header(
    DisplaySurface *surface,
    uint8_t page)
{
    const char *title = page == 0 ? "WEATHER" : "FORECAST";
    char page_index[8];
    snprintf(
        page_index,
        sizeof(page_index),
        "%u/%u",
        (unsigned)page + 1,
        WEATHER_PAGE_COUNT);
    ui_draw_header_with_detail(surface, title, page_index);
}

static void render_weather_settings(
    const WeatherAppState *state,
    DisplaySurface *surface)
{
    ui_draw_header(surface, "WEATHER SETTINGS");
    ui_draw_menu_item(
        surface,
        use_expanded_layout(surface) ? 48 : 38,
        "Themes",
        weather_theme_name(state->theme),
        true,
        true);
    ui_draw_footer(surface, ">/CLICK:OPEN  </B:BACK");
}

static void render_theme_selection(
    const WeatherAppState *state,
    DisplaySurface *surface)
{
    ui_draw_header(surface, "THEMES");
    for (size_t i = 0; i < WEATHER_THEME_COUNT; i++)
    {
        const WeatherTheme theme = (WeatherTheme)i;
        ui_draw_menu_item(
            surface,
            (use_expanded_layout(surface) ? 48 : 38) + (uint16_t)i * 32,
            weather_theme_name(theme),
            theme == state->theme ? "Active" : "",
            theme == state->selected_theme,
            true);
    }
    ui_draw_footer(surface, "UP/DN:MOVE CLICK:APPLY </B:BACK");
}

static void weather_render(
    const void *state_memory,
    DisplaySurface *surface)
{
    const WeatherAppState *state = state_memory;
    display_clear(surface, DISPLAY_COLOR_BLACK);
    if (state->screen == WEATHER_SCREEN_SETTINGS)
    {
        render_weather_settings(state, surface);
        return;
    }
    if (state->screen == WEATHER_SCREEN_THEMES)
    {
        render_theme_selection(state, surface);
        return;
    }

    render_weather_header(surface, state->page);

    if (state->weather.status == WEATHER_STATUS_READY)
    {
        if (state->page == 0)
        {
            render_current(state, surface);
        }
        else
        {
            render_forecast_page(state, surface, state->page);
        }
    }
    else
    {
        const bool connected =
            state->network.status == NETWORK_STATUS_CONNECTED;
        const char *message = connected
            ? weather_status_name(state->weather.status)
            : "Wi-Fi offline";
        DisplayColor color = ui_primary_text_color();
        if (state->weather.status == WEATHER_STATUS_LOADING)
        {
            color = ui_activity_color();
        }
        else if (state->weather.status == WEATHER_STATUS_ERROR)
        {
            color = ui_error_color();
        }
        display_draw_text_with_font(
            surface,
            centered_x(surface, message, DISPLAY_FONT_MEDIUM),
            use_expanded_layout(surface) ? 96 : 55,
            message,
            DISPLAY_FONT_MEDIUM,
            color,
            DISPLAY_COLOR_BLACK);
        if (!connected)
        {
            const char *hint = "Connect in Settings";
            display_draw_text(
                surface,
                centered_x(surface, hint, DISPLAY_FONT_SMALL),
                use_expanded_layout(surface) ? 128 : 82,
                hint,
                ui_secondary_text_color(),
                DISPLAY_COLOR_BLACK);
        }
    }

    ui_draw_footer(surface, "<>:PAGE CLK:SET A:REF B:HOME");
}

const Application weather_application = {
    .name = "Weather",
    .required_capabilities =
        APP_CAP_DISPLAY | APP_CAP_INPUT | APP_CAP_WIFI,
    .state_size = sizeof(WeatherAppState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = weather_start,
    .pause = NULL,
    .resume = weather_resume,
    .stop = NULL,
    .handle_event = weather_handle_event,
    .update = weather_update,
    .render = weather_render,
};
