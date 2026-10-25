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
    MAX_SPRITES = 8,
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
    128, 236, 344, 472, 596, 708, 844, 956,
};

static int positive_mod(int value, int divisor)
{
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
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
    enum TileKind tile)
{
    switch (tile)
    {
        case TILE_GRASS:
            display_fill_rectangle(
                surface, x, y, TILE_SIZE, TILE_SIZE, RGB565(88, 157, 76));
            display_fill_rectangle(
                surface, x, y + 3, TILE_SIZE, 5, RGB565(112, 76, 48));
            display_fill_rectangle(
                surface, x + 2, y + 4, 2, 2, RGB565(151, 103, 61));
            break;
        case TILE_DIRT:
            display_fill_rectangle(
                surface, x, y, TILE_SIZE, TILE_SIZE, RGB565(112, 76, 48));
            display_fill_rectangle(
                surface, x + 1, y + 2, 2, 2, RGB565(151, 103, 61));
            display_fill_rectangle(
                surface, x + 5, y + 6, 2, 1, RGB565(72, 50, 39));
            break;
        case TILE_BRICK:
            display_fill_rectangle(
                surface, x, y, TILE_SIZE, TILE_SIZE, RGB565(190, 96, 55));
            display_fill_rectangle(
                surface, x, y + 3, TILE_SIZE, 1, RGB565(83, 53, 48));
            display_fill_rectangle(
                surface, x + 3, y, 1, 3, RGB565(83, 53, 48));
            display_fill_rectangle(
                surface, x + 5, y + 4, 1, 4, RGB565(83, 53, 48));
            break;
        case TILE_SKY:
        default:
            break;
    }
}

static int ground_pixel_y(int world_x, int vertical_offset)
{
    return HUD_HEIGHT + vertical_offset +
        ground_row(world_x / TILE_SIZE) * TILE_SIZE;
}

static void draw_background(
    DisplaySurface *surface,
    int viewport_width,
    int viewport_height,
    int camera_x)
{
    display_fill_rectangle(
        surface,
        0,
        HUD_HEIGHT,
        viewport_width,
        viewport_height - HUD_HEIGHT,
        RGB565(105, 191, 224));

    const int parallax_offset = (camera_x / 4) % 32;
    for (int x = -parallax_offset - 32; x < viewport_width + 32; x += 32)
    {
        const int variation = positive_mod((x + camera_x / 4) / 32, 3);
        const int hill_height = 18 + variation * 7;
        display_fill_rectangle(
            surface,
            x,
            viewport_height - hill_height,
            33,
            hill_height,
            RGB565(73, 139, 112));
    }

    const int cloud_offset = (camera_x / 8) % 80;
    for (int x = 20 - cloud_offset; x < viewport_width; x += 80)
    {
        display_fill_rectangle(
            surface, x, HUD_HEIGHT + 17, 22, 5, RGB565(220, 244, 245));
        display_fill_rectangle(
            surface, x + 5, HUD_HEIGHT + 13, 11, 4, RGB565(220, 244, 245));
    }
}

static void draw_tiles(
    DisplaySurface *surface,
    int viewport_width,
    int camera_x,
    int vertical_offset)
{
    const int first_column = camera_x / TILE_SIZE;
    const int first_x = -(camera_x % TILE_SIZE);
    const int rows =
        (VIEWPORT_HEIGHT - HUD_HEIGHT + TILE_SIZE - 1) / TILE_SIZE;
    const int columns = (viewport_width + TILE_SIZE * 2 - 1) / TILE_SIZE;

    for (int column = 0; column < columns; column++)
    {
        const int world_column = first_column + column;
        const int x = first_x + column * TILE_SIZE;
        for (int row = 0; row < rows; row++)
        {
            draw_tile(
                surface,
                x,
                HUD_HEIGHT + vertical_offset + row * TILE_SIZE,
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
    int vertical_offset)
{
    const uint8_t sprite_scale = vertical_offset > 0 ? 2 : 1;
    const int player_world_x = positive_mod(state->camera_x + 56, WORLD_WIDTH);
    draw_runner(
        surface,
        56,
        ground_pixel_y(player_world_x, vertical_offset) - 12 * sprite_scale,
        state->animation_frame,
        player_palette,
        state->direction < 0,
        sprite_scale);

    for (uint8_t i = 0; i < state->sprite_count; i++)
    {
        int x = (int)sprite_world_x[i] - state->camera_x;
        x = positive_mod(x + 16, WORLD_WIDTH) - 16;
        if (x < -8 * sprite_scale || x >= viewport_width)
        {
            continue;
        }
        draw_runner(
            surface,
            x,
            ground_pixel_y(sprite_world_x[i], vertical_offset) -
                12 * sprite_scale,
            (uint8_t)(state->animation_frame + i),
            enemy_palettes[i % 3],
            (i & 1u) != 0,
            sprite_scale);
    }
}

static bool demo_start(void *state_memory, AppContext *context)
{
    (void)context;
    PlatformerDemoState *state = state_memory;
    state->direction = 1;
    state->speed = 2;
    state->sprite_count = MAX_SPRITES;
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
        state->sprite_count = state->sprite_count == MAX_SPRITES
            ? 4
            : MAX_SPRITES;
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
    const int viewport_width = surface->width < VIEWPORT_WIDTH
        ? surface->width
        : VIEWPORT_WIDTH;
    const int viewport_height = surface->height;
    const int vertical_offset = viewport_height > VIEWPORT_HEIGHT
        ? viewport_height - VIEWPORT_HEIGHT
        : 0;

    draw_background(
        surface, viewport_width, viewport_height, state->camera_x);
    draw_tiles(surface, viewport_width, state->camera_x, vertical_offset);
    draw_sprites(state, surface, viewport_width, vertical_offset);

    display_fill_rectangle(
        surface, 0, 0, viewport_width, HUD_HEIGHT, RGB565(20, 25, 35));
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
        2,
        2,
        status,
        DISPLAY_FONT_TINY,
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
