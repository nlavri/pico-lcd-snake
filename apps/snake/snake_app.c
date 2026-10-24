#include "snake_app.h"

#include <stdio.h>

#include "platform/time.h"
#include "snake_game.h"
#include "ui.h"

#define GAME_BORDER_WIDTH 2
#define GAME_LEFT_OFFSET 5
#define GAME_RIGHT_MARGIN 5
#define GAME_TOP_OFFSET 17
#define SNAKE_UPDATE_INTERVAL_MS 40

enum
{
    SNAKE_OUTLINE_COLOR = 0x7800,
    SNAKE_BODY_COLOR = 0xF800,
    SNAKE_HIGHLIGHT_COLOR = 0xFB4D,
    SNAKE_TONGUE_COLOR = 0xF81F,
    FRUIT_OUTLINE_COLOR = 0x0320,
    FRUIT_BODY_COLOR = 0x07E0,
    FRUIT_HIGHLIGHT_COLOR = 0xBFE0,
    FRUIT_STEM_COLOR = 0xA145,
};

typedef struct SnakeAppState
{
    SnakeGameState game;
    SnakeGameInput pending_input;
    uint32_t elapsed_ms;
} SnakeAppState;

static uint32_t snake_random(void *context)
{
    (void)context;
    return platform_random_u32();
}

static SnakeDirection map_direction(InputButton button)
{
    if (button == INPUT_UP)
    {
        return SNAKE_DIRECTION_UP;
    }
    if (button == INPUT_DOWN)
    {
        return SNAKE_DIRECTION_DOWN;
    }
    if (button == INPUT_LEFT)
    {
        return SNAKE_DIRECTION_LEFT;
    }
    if (button == INPUT_RIGHT)
    {
        return SNAKE_DIRECTION_RIGHT;
    }
    return SNAKE_DIRECTION_NONE;
}

static bool snake_start(void *state_memory, AppContext *context)
{
    SnakeAppState *state = state_memory;
    const uint8_t field_width =
        context->display_width - GAME_LEFT_OFFSET - GAME_RIGHT_MARGIN -
        (2 * GAME_BORDER_WIDTH);
    const uint8_t field_height =
        context->display_height - GAME_TOP_OFFSET - (2 * GAME_BORDER_WIDTH);

    return snake_game_init(
        &state->game,
        field_width,
        field_height,
        snake_random,
        NULL);
}

static AppResult snake_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    SnakeAppState *state = state_memory;
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    const SnakeDirection direction = map_direction(event->button);
    if (direction != SNAKE_DIRECTION_NONE)
    {
        state->pending_input.direction = direction;
    }
    else if (event->button == INPUT_A)
    {
        state->pending_input.increase_speed = true;
    }
    else if (event->button == INPUT_B)
    {
        state->pending_input.decrease_speed = true;
    }

    return APP_RESULT_CONTINUE;
}

static AppResult snake_update(void *state_memory, uint32_t elapsed_ms)
{
    SnakeAppState *state = state_memory;
    state->elapsed_ms += elapsed_ms;

    if (state->elapsed_ms < SNAKE_UPDATE_INTERVAL_MS)
    {
        return APP_RESULT_CONTINUE;
    }

    state->elapsed_ms %= SNAKE_UPDATE_INTERVAL_MS;
    snake_game_update(&state->game, &state->pending_input);
    state->pending_input = (SnakeGameInput){0};
    return APP_RESULT_REDRAW;
}

static void fill_pixel_rectangle(
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
                // The Waveshare drawing primitive treats its coordinate as
                // one past the top-left pixel for a one-pixel point.
                display_draw_point(
                    surface,
                    (uint16_t)(target_x + 1),
                    (uint16_t)(target_y + 1),
                    color,
                    1);
            }
        }
    }
}

static void draw_body_layer(
    DisplaySurface *surface,
    const SnakeGameState *game,
    int origin_x,
    int origin_y,
    bool inner)
{
    for (uint16_t i = game->snake_length; i-- > 1;)
    {
        const uint16_t distance_from_tail = game->snake_length - 1 - i;
        int size = game->snake_size;
        if (distance_from_tail + 2 < game->snake_size)
        {
            size = distance_from_tail + 2;
        }

        int inset = (game->snake_size - size) / 2;
        if (inner)
        {
            if (size <= 2)
            {
                continue;
            }
            inset++;
            size -= 2;
        }

        fill_pixel_rectangle(
            surface,
            origin_x + game->snake_body[i].x + inset,
            origin_y + game->snake_body[i].y + inset,
            size,
            size,
            inner ? SNAKE_BODY_COLOR : SNAKE_OUTLINE_COLOR);
    }
}

static void draw_body_highlights(
    DisplaySurface *surface,
    const SnakeGameState *game,
    int origin_x,
    int origin_y)
{
    const uint16_t spacing = game->snake_size;
    for (uint16_t i = spacing / 2; i < game->snake_length; i += spacing)
    {
        fill_pixel_rectangle(
            surface,
            origin_x + game->snake_body[i].x + 2,
            origin_y + game->snake_body[i].y + 2,
            2,
            1,
            SNAKE_HIGHLIGHT_COLOR);
    }
}

static void draw_eye(
    DisplaySurface *surface,
    int x,
    int y,
    int pupil_x,
    int pupil_y)
{
    fill_pixel_rectangle(surface, x, y, 2, 2, DISPLAY_COLOR_WHITE);
    fill_pixel_rectangle(surface, pupil_x, pupil_y, 1, 1, DISPLAY_COLOR_BLACK);
}

static void draw_snake_head(
    DisplaySurface *surface,
    const SnakeGameState *game,
    int origin_x,
    int origin_y)
{
    const int x = origin_x + game->snake_body[0].x;
    const int y = origin_y + game->snake_body[0].y;
    const int size = game->snake_size;
    fill_pixel_rectangle(surface, x, y, size, size, SNAKE_OUTLINE_COLOR);
    fill_pixel_rectangle(
        surface, x + 1, y + 1, size - 2, size - 2, SNAKE_BODY_COLOR);
    fill_pixel_rectangle(
        surface, x + 2, y + 1, 2, 1, SNAKE_HIGHLIGHT_COLOR);

    switch (game->current_direction)
    {
        case SNAKE_DIRECTION_LEFT:
            draw_eye(surface, x + 1, y + 1, x + 1, y + 2);
            draw_eye(surface, x + 1, y + 5, x + 1, y + 5);
            if (game->snake_body[0].x >= 2)
            {
                fill_pixel_rectangle(
                    surface, x - 2, y + 4, 2, 1, SNAKE_TONGUE_COLOR);
            }
            break;
        case SNAKE_DIRECTION_UP:
            draw_eye(surface, x + 1, y + 1, x + 1, y + 1);
            draw_eye(surface, x + 5, y + 1, x + 6, y + 1);
            if (game->snake_body[0].y >= 2)
            {
                fill_pixel_rectangle(
                    surface, x + 4, y - 2, 1, 2, SNAKE_TONGUE_COLOR);
            }
            break;
        case SNAKE_DIRECTION_DOWN:
            draw_eye(surface, x + 1, y + 5, x + 1, y + 6);
            draw_eye(surface, x + 5, y + 5, x + 6, y + 6);
            if (game->snake_body[0].y + size + 2 <= game->field_height)
            {
                fill_pixel_rectangle(
                    surface, x + 4, y + size, 1, 2, SNAKE_TONGUE_COLOR);
            }
            break;
        case SNAKE_DIRECTION_RIGHT:
            draw_eye(surface, x + 5, y + 1, x + 6, y + 2);
            draw_eye(surface, x + 5, y + 5, x + 6, y + 5);
            if (game->snake_body[0].x + size + 2 <= game->field_width)
            {
                fill_pixel_rectangle(
                    surface, x + size, y + 4, 2, 1, SNAKE_TONGUE_COLOR);
            }
            break;
        case SNAKE_DIRECTION_NONE:
        default:
            draw_eye(surface, x + 5, y + 1, x + 6, y + 2);
            draw_eye(surface, x + 5, y + 5, x + 6, y + 5);
            break;
    }
}

static void draw_pixel_fruit(
    DisplaySurface *surface,
    const SnakeGameState *game,
    int origin_x,
    int origin_y)
{
    const int x = origin_x + game->fruit_position.x;
    const int y = origin_y + game->fruit_position.y;
    fill_pixel_rectangle(surface, x + 3, y, 2, 2, FRUIT_STEM_COLOR);
    fill_pixel_rectangle(surface, x + 5, y + 1, 2, 1, FRUIT_BODY_COLOR);
    fill_pixel_rectangle(surface, x + 2, y + 1, 4, 1, FRUIT_OUTLINE_COLOR);
    fill_pixel_rectangle(surface, x + 1, y + 2, 6, 1, FRUIT_OUTLINE_COLOR);
    fill_pixel_rectangle(surface, x, y + 3, 8, 3, FRUIT_OUTLINE_COLOR);
    fill_pixel_rectangle(surface, x + 1, y + 6, 6, 1, FRUIT_OUTLINE_COLOR);
    fill_pixel_rectangle(surface, x + 2, y + 7, 4, 1, FRUIT_OUTLINE_COLOR);
    fill_pixel_rectangle(surface, x + 2, y + 2, 4, 4, FRUIT_BODY_COLOR);
    fill_pixel_rectangle(surface, x + 1, y + 3, 1, 2, FRUIT_BODY_COLOR);
    fill_pixel_rectangle(surface, x + 6, y + 3, 1, 2, FRUIT_BODY_COLOR);
    fill_pixel_rectangle(surface, x + 2, y + 3, 2, 2, FRUIT_HIGHLIGHT_COLOR);
}

static void snake_render(const void *state_memory, DisplaySurface *surface)
{
    const SnakeAppState *state = state_memory;
    char status[40];
    snprintf(
        status,
        sizeof(status),
        "SNAKE SCORE:%lu SPEED:%u",
        (unsigned long)state->game.score,
        state->game.speed);

    display_clear(surface, DISPLAY_COLOR_BLACK);
    ui_draw_header(surface, status);
    display_draw_rectangle(
        surface,
        GAME_LEFT_OFFSET,
        GAME_TOP_OFFSET,
        surface->width - GAME_RIGHT_MARGIN - 1,
        surface->height - 1,
        ui_accent_color(),
        GAME_BORDER_WIDTH);

    const int field_x = GAME_LEFT_OFFSET + GAME_BORDER_WIDTH;
    const int field_y = GAME_TOP_OFFSET + GAME_BORDER_WIDTH;
    draw_body_layer(surface, &state->game, field_x, field_y, false);
    draw_body_layer(surface, &state->game, field_x, field_y, true);
    draw_body_highlights(surface, &state->game, field_x, field_y);
    draw_snake_head(surface, &state->game, field_x, field_y);
    draw_pixel_fruit(surface, &state->game, field_x, field_y);
}

const Application snake_application = {
    .name = "Snake",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(SnakeAppState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = snake_start,
    .pause = NULL,
    .resume = NULL,
    .stop = NULL,
    .handle_event = snake_handle_event,
    .update = snake_update,
    .render = snake_render,
};
