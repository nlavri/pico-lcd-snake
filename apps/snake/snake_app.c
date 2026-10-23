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

    for (uint16_t i = 0; i < state->game.snake_length; i++)
    {
        display_draw_point(
            surface,
            GAME_LEFT_OFFSET + GAME_BORDER_WIDTH + state->game.snake_body[i].x,
            GAME_TOP_OFFSET + GAME_BORDER_WIDTH + state->game.snake_body[i].y,
            DISPLAY_COLOR_RED,
            state->game.snake_size);
    }

    display_draw_point(
        surface,
        GAME_LEFT_OFFSET + GAME_BORDER_WIDTH + state->game.fruit_position.x,
        GAME_TOP_OFFSET + GAME_BORDER_WIDTH + state->game.fruit_position.y,
        DISPLAY_COLOR_GREEN,
        state->game.fruit_size);
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
