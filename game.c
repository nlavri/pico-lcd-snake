#include "game.h"
#include "input_hal.h"
#include "output_hal.h"
#include "runtime.h"

void set_random_fruit_position(GameState *const state)
{
    while ((state->fruit_position.x = get_random() % state->field_w) == 0)
        ;
    while ((state->fruit_position.y = get_random() % state->field_h) == 0)
        ;
}

void init_game_state(GameState *const state)
{
    state->exit_requested = 0;
    state->is_paused = 0;

    state->score = 0;

    uint8_t field_h = get_field_h();
    uint8_t field_w = get_field_w();

    state->field_h = field_h;
    state->field_w = field_w;

    state->snake_position.x = field_w / 2;
    state->snake_position.y = field_h / 2;

    state->current_direction = MOV_DIR_UNDEFINED;

    set_random_fruit_position(state);
}

void init_input_state(GameInput *const input)
{
    input->direction = MOV_DIR_UNDEFINED;
    input->control_button = false;
}

void get_input(GameInput *const input)
{
    input->direction = get_direction_input();
    input->control_button = get_control_button_state();
}

void update_state(GameState *const state, GameInput *const input)
{
    if (input->direction != MOV_DIR_UNDEFINED)
    {
        state->current_direction = input->direction;
    }

    switch (state->current_direction)
    {
    case MOV_DIR_LEFT:
        if (state->snake_position.x > 0)
            state->snake_position.x--;
        break;
    case MOV_DIR_UP:
        if (state->snake_position.y > 0)
            state->snake_position.y--;
        break;
    case MOV_DIR_RIGHT:
        if (state->snake_position.x < state->field_w)
            state->snake_position.x++;
        break;
    case MOV_DIR_DOWN:
        if (state->snake_position.y < state->field_h)
            state->snake_position.y++;
        break;
    default:
        break;
    }
}

int init_game(GameState *const state, GameInput *const input)
{
    init_game_state(state);

    init_input_state(input);

    int init_output_result = init_output();
    if(init_output_result != 0)
        return init_output_result;
    
    init_input();

    return 0;
}

void main_loop(GameState *const state, GameInput *const input)
{
    while (!state->exit_requested)
    {
        draw_state(state, input);

        sleep_milliseconds(250);

        get_input(input);

        update_state(state, input);
    }
}