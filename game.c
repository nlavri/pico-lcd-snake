#include <stdio.h>

#include "game.h"
#include "input_hal.h"
#include "output_hal.h"
#include "runtime.h"

uint8_t max_x = 0;
uint8_t max_y = 0;

void set_random_fruit_position(GameState *const state)
{
    state->prev_fruit_position.x = state->fruit_position.x;
    state->prev_fruit_position.y = state->fruit_position.y;

    while ((state->fruit_position.x = get_random() % (state->field_w - state->snake_size)) == 0)
        ;
    while ((state->fruit_position.y = get_random() % (state->field_h - state->snake_size)) == 0)
        ;
}

void init_game_state(GameState *const state, OutputInitResult *const output)
{
    state->exit_requested = 0;
    state->is_paused = 0;

    state->score = 0;

    uint8_t field_h = output->h;
    uint8_t field_w = output->w;

    state->field_h = field_h;
    state->field_w = field_w;

    state->snake_position.x = field_w / 2;
    state->snake_position.y = field_h / 2;
    state->snake_size = 8;
    state->prev_snake_position.x = state->snake_position.x;
    state->prev_snake_position.y = state->snake_position.y;

    state->current_direction = MOV_DIR_UNDEFINED;

    set_random_fruit_position(state);
    state->prev_fruit_position.x = state->fruit_position.x;
    state->prev_fruit_position.y = state->fruit_position.y;

    state->fruit_size = 8;

    state->speed = 1;

    max_x = state->field_w - state->snake_size;
    max_y = state->field_h - state->snake_size;
}

void init_input_state(GameInput *const input)
{
    input->direction = MOV_DIR_UNDEFINED;
    input->btn_ctrl = false;
}

void get_input(GameInput *const input)
{
    input->direction = get_direction_input();
    input->btn_ctrl = get_control_button_state();
    input->btn_a = get_key_a_state();
    input->btn_b = get_key_b_state();
}

void move_snake(GameState *const state)
{
    state->prev_snake_position.x = state->snake_position.x;
    state->prev_snake_position.y = state->snake_position.y;

    switch (state->current_direction)
    {
    case MOV_DIR_LEFT:
        if (state->snake_position.x > 0)
            state->snake_position.x -= state->speed;
        // check overflow
        if (state->snake_position.x > state->prev_snake_position.x)
            state->snake_position.x = 0;
        break;
    case MOV_DIR_UP:
        if (state->snake_position.y > 0)
            state->snake_position.y -= state->speed;
        // check overflow
        if (state->snake_position.y > state->prev_snake_position.y)
            state->snake_position.y = 0;
        break;
    case MOV_DIR_RIGHT:
        if (state->snake_position.x < max_x)
            state->snake_position.x += state->speed;
        if (state->snake_position.x > max_x)
            state->snake_position.x = max_x;
        break;
    case MOV_DIR_DOWN:
        if (state->snake_position.y < max_y)
            state->snake_position.y += state->speed;
        if (state->snake_position.y > max_y)
            state->snake_position.y = max_y;
        break;
    default:
        break;
    }
}

void check_fruit_eaten(GameState *const state)
{
    // snake.X1 < fruit.X2 && snake.X2 > fruit.X1 &&
    // snake.Y1 < fruit.Y2 && snake.Y2 > fruit.Y1)
    uint8_t snake_x2 = state->snake_position.x + state->snake_size;
    uint8_t snake_y2 = state->snake_position.y + state->snake_size;
    uint8_t fruit_x2 = state->fruit_position.x + state->snake_size;
    uint8_t fruit_y2 = state->fruit_position.y + state->snake_size;

    if (state->snake_position.x < fruit_x2 && snake_x2 > state->fruit_position.x &&
        state->snake_position.y < fruit_y2 && snake_y2 > state->fruit_position.y)
    {
        state->score++;
        set_random_fruit_position(state);
    }
}

uint8_t speed_bouncer = 0;
void update_speed(GameState *const state, GameInput *const input)
{
    if (speed_bouncer == 0)
    {
        if (input->btn_a && state->speed < 8)
            state->speed++;
        else if (input->btn_b && state->speed > 0)
            state->speed--;
    }

    speed_bouncer++;
    if (speed_bouncer == 4)
        speed_bouncer = 0;
}

void update_state(GameState *const state, GameInput *const input)
{
    if (input->btn_ctrl)
        state->exit_requested = 1;

    if (input->direction != MOV_DIR_UNDEFINED)
        state->current_direction = input->direction;

    update_speed(state, input);

    check_fruit_eaten(state);

    move_snake(state);
}

int init_game(GameState *const state, GameInput *const input)
{
    OutputInitResult init_output_result = init_output(state);
    if (init_output_result.ok == false)
        return -1;

    init_game_state(state, &init_output_result);

    post_init_output(state);

    init_input_state(input);

    init_input();

    printf("game initialized");

    return 0;
}

void main_loop(GameState *const state, GameInput *const input)
{
    while (!state->exit_requested)
    {
        draw_state(state, input);

        // sleep_milliseconds(10);

        get_input(input);

        update_state(state, input);

        if (state->exit_requested)
        {
            printf("Exit requested \r\n");
            return;
        }
    }
}