#include "platformer_demo_app.h"

#include <stdio.h>

#include "common/logger.h"
#include "platform/time.h"

#define RGB565(red, green, blue) ((DisplayColor)( \
    (((uint16_t)(red) & 0xf8u) << 8) | \
    (((uint16_t)(green) & 0xfcu) << 3) | \
    ((uint16_t)(blue) >> 3)))

enum
{
    VIEWPORT_WIDTH = 240,
    VIEWPORT_HEIGHT = 135,
    HUD_HEIGHT = 13,
    TILE_SIZE = 8,
    WORLD_TILE_COLUMNS = 128,
    WORLD_WIDTH = WORLD_TILE_COLUMNS * TILE_SIZE,
    FRAME_INTERVAL_MS = 16,
    ANIMATION_INTERVAL_MS = 120,
    MAX_SPRITES = 12,
};

enum TileKind
{
    TILE_SKY,
    TILE_GRASS,
    TILE_DIRT,
    TILE_BRICK,
};

typedef struct PlatformerDemoState
{
    uint32_t frame_elapsed_ms;
    uint32_t animation_elapsed_ms;
    int32_t camera_x;
    int8_t direction;
    uint8_t speed;
    uint8_t animation_frame;
    uint8_t sprite_count;
    bool large_display;
    bool paused;
    uint32_t render_count;
    uint32_t last_render_us;
    uint32_t maximum_render_us;
} PlatformerDemoState;

_Static_assert(
    sizeof(PlatformerDemoState) <= 2048,
    "Platformer demo state must stay inside its 2 KiB budget");

static const uint8_t runner_frames[2][12][8] = {
    {
        {0, 0, 1, 1, 1, 1, 0, 0},
        {0, 1, 2, 2, 2, 2, 1, 0},
        {0, 1, 3, 2, 2, 3, 1, 0},
        {0, 1, 2, 2, 2, 2, 1, 0},
        {0, 0, 1, 2, 2, 1, 0, 0},
        {0, 1, 1, 2, 2, 1, 1, 0},
        {1, 1, 0, 2, 2, 0, 1, 1},
        {0, 0, 0, 2, 2, 0, 0, 0},
        {0, 0, 1, 2, 2, 1, 0, 0},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {0, 1, 0, 0, 0, 0, 1, 0},
        {1, 1, 0, 0, 0, 0, 1, 1},
    },
    {
        {0, 0, 1, 1, 1, 1, 0, 0},
        {0, 1, 2, 2, 2, 2, 1, 0},
        {0, 1, 3, 2, 2, 3, 1, 0},
        {0, 1, 2, 2, 2, 2, 1, 0},
        {0, 0, 1, 2, 2, 1, 0, 0},
        {0, 1, 1, 2, 2, 1, 1, 0},
        {1, 1, 0, 2, 2, 0, 1, 1},
        {0, 0, 0, 2, 2, 0, 0, 0},
        {0, 0, 1, 2, 2, 1, 0, 0},
        {0, 0, 1, 0, 0, 1, 0, 0},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {0, 1, 0, 0, 0, 0, 1, 0},
    },
};

static const DisplayColor player_palette[] = {
    0,
    RGB565(35, 31, 42),
    RGB565(242, 91, 72),
    RGB565(255, 232, 166),
};

static const DisplayColor enemy_palettes[3][4] = {
    {0, RGB565(29, 36, 46), RGB565(91, 192, 235), RGB565(235, 249, 255)},
    {0, RGB565(45, 31, 52), RGB565(192, 103, 224), RGB565(255, 224, 250)},
    {0, RGB565(42, 40, 25), RGB565(231, 185, 69), RGB565(255, 244, 184)},
};

static const uint16_t sprite_world_x[MAX_SPRITES] = {
    96, 176, 260, 344, 428, 512,
    596, 680, 764, 848, 932, 1008,
};

static int positive_mod(int value, int divisor)
{
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

static int scale_world(int value, int numerator, int denominator)
{
    return value * numerator / denominator;
}

static int ground_row(int world_column)
{
    static const uint8_t profile[16] = {
        12, 12, 12, 11, 11, 11, 12, 12,
        13, 13, 12, 12, 11, 11, 12, 12,
    };
    return profile[positive_mod(world_column, 16)];
}

static enum TileKind tile_at(int world_column, int row)
{
    const int wrapped_column = positive_mod(world_column, WORLD_TILE_COLUMNS);
    const int ground = ground_row(wrapped_column);
    if (row == ground)
    {
        return TILE_GRASS;
    }
    if (row > ground)
    {
        return TILE_DIRT;
    }

    const int section = wrapped_column % 32;
    if ((row == 8 && section >= 5 && section <= 10) ||
        (row == 6 && section >= 20 && section <= 23))
    {
        return TILE_BRICK;
    }
    return TILE_SKY;
}

static void draw_tile(
    DisplaySurface *surface,
    int x,
    int y,
    int tile_size,
    enum TileKind tile)
{
    const int grass_depth = tile_size * 3 / TILE_SIZE;
    const int accent = tile_size >= 12 ? 3 : 2;
    switch (tile)
    {
        case TILE_GRASS:
            display_fill_rectangle(
                surface, x, y, tile_size, tile_size, RGB565(88, 157, 76));
            display_fill_rectangle(
                surface,
                x,
                y + grass_depth,
                tile_size,
                tile_size - grass_depth,
                RGB565(112, 76, 48));
            display_fill_rectangle(
                surface,
                x + tile_size / 4,
                y + tile_size / 2,
                accent,
                accent,
                RGB565(151, 103, 61));
            break;
        case TILE_DIRT:
            display_fill_rectangle(
                surface, x, y, tile_size, tile_size, RGB565(112, 76, 48));
            display_fill_rectangle(
                surface,
                x + tile_size / 8,
                y + tile_size / 4,
                accent,
                accent,
                RGB565(151, 103, 61));
            display_fill_rectangle(
                surface,
                x + tile_size * 5 / 8,
                y + tile_size * 3 / 4,
                accent,
                tile_size >= 12 ? 2 : 1,
                RGB565(72, 50, 39));
            break;
        case TILE_BRICK:
            display_fill_rectangle(
                surface, x, y, tile_size, tile_size, RGB565(190, 96, 55));
            display_fill_rectangle(
                surface,
                x,
                y + tile_size * 3 / 8,
                tile_size,
                tile_size >= 12 ? 2 : 1,
                RGB565(83, 53, 48));
            display_fill_rectangle(
                surface,
                x + tile_size * 3 / 8,
                y,
                tile_size >= 12 ? 2 : 1,
                tile_size * 3 / 8,
                RGB565(83, 53, 48));
            display_fill_rectangle(
                surface,
                x + tile_size * 5 / 8,
                y + tile_size / 2,
                tile_size >= 12 ? 2 : 1,
                tile_size / 2,
                RGB565(83, 53, 48));
            break;
        case TILE_SKY:
        default:
            break;
    }
}

static int ground_pixel_y(int world_x, int world_origin_y, int tile_size)
{
    return world_origin_y + ground_row(world_x / TILE_SIZE) * tile_size;
}

static void draw_background(
    DisplaySurface *surface,
    int viewport_width,
    int viewport_height,
    int camera_x,
    int hud_height,
    int scale_numerator,
    int scale_denominator)
{
    display_fill_rectangle(
        surface,
        0,
        hud_height,
        viewport_width,
        viewport_height - hud_height,
        RGB565(105, 191, 224));

    const int hill_step = scale_world(
        32, scale_numerator, scale_denominator);
    const int parallax_offset = scale_world(
        camera_x / 4, scale_numerator, scale_denominator) % hill_step;
    for (int x = -parallax_offset - hill_step;
         x < viewport_width + hill_step;
         x += hill_step)
    {
        const int variation = positive_mod(
            (x + parallax_offset) / hill_step, 3);
        const int hill_height = scale_world(
            18 + variation * 7, scale_numerator, scale_denominator);
        display_fill_rectangle(
            surface,
            x,
            viewport_height - hill_height,
            hill_step + 1,
            hill_height,
            RGB565(73, 139, 112));
    }

    const int cloud_step = scale_world(
        80, scale_numerator, scale_denominator);
    const int cloud_offset = scale_world(
        camera_x / 8, scale_numerator, scale_denominator) % cloud_step;
    for (int x = scale_world(20, scale_numerator, scale_denominator) -
             cloud_offset;
         x < viewport_width;
         x += cloud_step)
    {
        display_fill_rectangle(
            surface,
            x,
            hud_height + scale_world(17, scale_numerator, scale_denominator),
            scale_world(22, scale_numerator, scale_denominator),
            scale_world(5, scale_numerator, scale_denominator),
            RGB565(220, 244, 245));
        display_fill_rectangle(
            surface,
            x + scale_world(5, scale_numerator, scale_denominator),
            hud_height + scale_world(13, scale_numerator, scale_denominator),
            scale_world(11, scale_numerator, scale_denominator),
            scale_world(4, scale_numerator, scale_denominator),
            RGB565(220, 244, 245));
    }
}

static void draw_tiles(
    DisplaySurface *surface,
    int viewport_width,
    int camera_x,
    int world_origin_y,
    int tile_size,
    int scale_numerator,
    int scale_denominator)
{
    const int first_column = camera_x / TILE_SIZE;
    const int first_x = -scale_world(
        camera_x % TILE_SIZE, scale_numerator, scale_denominator);
    const int rows =
        (VIEWPORT_HEIGHT - HUD_HEIGHT + TILE_SIZE - 1) / TILE_SIZE;
    const int columns = (viewport_width + tile_size * 2 - 1) / tile_size;

    for (int column = 0; column < columns; column++)
    {
        const int world_column = first_column + column;
        const int x = first_x + column * tile_size;
        for (int row = 0; row < rows; row++)
        {
            draw_tile(
                surface,
                x,
                world_origin_y + row * tile_size,
                tile_size,
                tile_at(world_column, row));
        }
    }
}

static void draw_runner(
    DisplaySurface *surface,
    int x,
    int y,
    uint8_t frame,
    const DisplayColor *palette,
    bool flip_x,
    uint8_t scale)
{
    display_blit_indexed_scaled(
        surface,
        x,
        y,
        &runner_frames[frame & 1u][0][0],
        8,
        12,
        palette,
        4,
        0,
        flip_x,
        scale);
}

static void draw_sprites(
    const PlatformerDemoState *state,
    DisplaySurface *surface,
    int viewport_width,
    int world_origin_y,
    int tile_size,
    int scale_numerator,
    int scale_denominator,
    uint8_t sprite_scale)
{
    const int player_world_offset = state->large_display ? 80 : 56;
    const int player_x = scale_world(
        player_world_offset, scale_numerator, scale_denominator);
    const int player_world_x = positive_mod(
        state->camera_x + player_world_offset, WORLD_WIDTH);
    draw_runner(
        surface,
        player_x,
        ground_pixel_y(player_world_x, world_origin_y, tile_size) -
            12 * sprite_scale,
        state->animation_frame,
        player_palette,
        state->direction < 0,
        sprite_scale);

    for (uint8_t i = 0; i < state->sprite_count; i++)
    {
        int world_delta = positive_mod(
            (int)sprite_world_x[i] - state->camera_x, WORLD_WIDTH);
        if (world_delta > WORLD_WIDTH - 16)
        {
            world_delta -= WORLD_WIDTH;
        }
        const int x = scale_world(
            world_delta, scale_numerator, scale_denominator);
        if (x < -8 * sprite_scale || x >= viewport_width)
        {
            continue;
        }
        draw_runner(
            surface,
            x,
            ground_pixel_y(
                sprite_world_x[i], world_origin_y, tile_size) -
                12 * sprite_scale,
            (uint8_t)(state->animation_frame + i),
            enemy_palettes[i % 3],
            (i & 1u) != 0,
            sprite_scale);
    }
}

static bool demo_start(void *state_memory, AppContext *context)
{
    PlatformerDemoState *state = state_memory;
    state->large_display =
        context->display_width >= 400 && context->display_height >= 300;
    state->direction = 1;
    state->speed = 2;
    state->sprite_count = state->large_display ? MAX_SPRITES : 8;
    state->render_count = 0;
    state->last_render_us = 0;
    state->maximum_render_us = 0;
    return true;
}

static AppResult demo_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    PlatformerDemoState *state = state_memory;
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    if (event->button == INPUT_LEFT)
    {
        state->direction = -1;
    }
    else if (event->button == INPUT_RIGHT)
    {
        state->direction = 1;
    }
    else if (event->button == INPUT_UP && state->speed < 4)
    {
        state->speed++;
    }
    else if (event->button == INPUT_DOWN && state->speed > 1)
    {
        state->speed--;
    }
    else if (event->button == INPUT_CLICK)
    {
        state->paused = !state->paused;
    }
    else if (event->button == INPUT_A)
    {
        const uint8_t maximum = state->large_display ? MAX_SPRITES : 8;
        state->sprite_count = state->sprite_count == maximum
            ? 4
            : maximum;
    }
    else if (event->button == INPUT_B)
    {
        state->camera_x = 0;
    }
    return APP_RESULT_REDRAW;
}

static AppResult demo_update(void *state_memory, uint32_t elapsed_ms)
{
    PlatformerDemoState *state = state_memory;
    if (state->paused)
    {
        return APP_RESULT_CONTINUE;
    }

    state->frame_elapsed_ms += elapsed_ms;
    state->animation_elapsed_ms += elapsed_ms;
    bool changed = false;
    while (state->frame_elapsed_ms >= FRAME_INTERVAL_MS)
    {
        state->frame_elapsed_ms -= FRAME_INTERVAL_MS;
        state->camera_x = positive_mod(
            state->camera_x + state->direction * state->speed,
            WORLD_WIDTH);
        changed = true;
    }
    while (state->animation_elapsed_ms >= ANIMATION_INTERVAL_MS)
    {
        state->animation_elapsed_ms -= ANIMATION_INTERVAL_MS;
        state->animation_frame ^= 1u;
    }
    return changed ? APP_RESULT_REDRAW : APP_RESULT_CONTINUE;
}

static void demo_render(const void *state_memory, DisplaySurface *surface)
{
    /* Render timing is diagnostic state owned by this transient app instance. */
    PlatformerDemoState *state = (PlatformerDemoState *)state_memory;
    const uint32_t started_us = platform_time_us();
    const int scale_numerator = state->large_display ? 3 : 1;
    const int scale_denominator = state->large_display ? 2 : 1;
    const int tile_size = scale_world(
        TILE_SIZE, scale_numerator, scale_denominator);
    const uint8_t sprite_scale = state->large_display
        ? 3
        : surface->height > VIEWPORT_HEIGHT ? 2 : 1;
    const int hud_height = state->large_display ? 20 : HUD_HEIGHT;
    const int viewport_width = state->large_display
        ? surface->width
        : surface->width < VIEWPORT_WIDTH ? surface->width : VIEWPORT_WIDTH;
    const int viewport_height = surface->height;
    const int world_height = scale_world(
        VIEWPORT_HEIGHT - HUD_HEIGHT,
        scale_numerator,
        scale_denominator);
    const int world_origin_y = viewport_height > world_height
        ? viewport_height - world_height
        : hud_height;

    draw_background(
        surface,
        viewport_width,
        viewport_height,
        state->camera_x,
        hud_height,
        scale_numerator,
        scale_denominator);
    draw_tiles(
        surface,
        viewport_width,
        state->camera_x,
        world_origin_y,
        tile_size,
        scale_numerator,
        scale_denominator);
    draw_sprites(
        state,
        surface,
        viewport_width,
        world_origin_y,
        tile_size,
        scale_numerator,
        scale_denominator,
        sprite_scale);

    display_fill_rectangle(
        surface, 0, 0, viewport_width, hud_height, RGB565(20, 25, 35));
    char status[48];
    snprintf(
        status,
        sizeof(status),
        "%s S:%u N:%u R:%luus",
        state->paused ? "PAUSE" : "DEMO",
        state->speed,
        state->sprite_count,
        (unsigned long)state->last_render_us);
    display_draw_text_with_font(
        surface,
        state->large_display ? 5 : 2,
        state->large_display ? 4 : 2,
        status,
        state->large_display ? DISPLAY_FONT_SMALL : DISPLAY_FONT_TINY,
        DISPLAY_COLOR_WHITE,
        RGB565(20, 25, 35));

    state->last_render_us = platform_time_us() - started_us;
    if (state->last_render_us > state->maximum_render_us)
    {
        state->maximum_render_us = state->last_render_us;
    }
    state->render_count++;
    if (state->render_count % 120u == 0u)
    {
        LOG_INFO(
            "platformer",
            "render %lu us, maximum %lu us, sprites %u",
            (unsigned long)state->last_render_us,
            (unsigned long)state->maximum_render_us,
            state->sprite_count);
    }
}

const Application platformer_demo_application = {
    .name = "Platformer Demo",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(PlatformerDemoState),
    .state_policy = APP_STATE_TRANSIENT,
    .start = demo_start,
    .pause = NULL,
    .resume = NULL,
    .stop = NULL,
    .handle_event = demo_handle_event,
    .update = demo_update,
    .render = demo_render,
};
