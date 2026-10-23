#include "snake_game.h"

static bool rectangles_overlap(
    SnakePoint first,
    uint8_t first_size,
    SnakePoint second,
    uint8_t second_size)
{
    return first.x < second.x + second_size && first.x + first_size > second.x &&
           first.y < second.y + second_size && first.y + first_size > second.y;
}

static bool fruit_overlaps_snake(const SnakeGameState *state, SnakePoint fruit_position)
{
    for (uint16_t i = 0; i < state->snake_length; i++)
    {
        if (rectangles_overlap(
                state->snake_body[i], state->snake_size,
                fruit_position, state->fruit_size))
        {
            return true;
        }
    }

    return false;
}

static uint32_t next_random(SnakeGameState *state)
{
    return state->random_source(state->random_context);
}

static void place_fruit(SnakeGameState *state)
{
    do
    {
        state->fruit_position.x =
            next_random(state) % (state->field_width - state->fruit_size + 1);
        state->fruit_position.y =
            next_random(state) % (state->field_height - state->fruit_size + 1);
    } while (fruit_overlaps_snake(state, state->fruit_position));
}

static bool directions_are_opposite(SnakeDirection first, SnakeDirection second)
{
    return (first == SNAKE_DIRECTION_LEFT && second == SNAKE_DIRECTION_RIGHT) ||
           (first == SNAKE_DIRECTION_RIGHT && second == SNAKE_DIRECTION_LEFT) ||
           (first == SNAKE_DIRECTION_UP && second == SNAKE_DIRECTION_DOWN) ||
           (first == SNAKE_DIRECTION_DOWN && second == SNAKE_DIRECTION_UP);
}

static void update_speed(SnakeGameState *state, const SnakeGameInput *input)
{
    if (input->increase_speed && state->speed < state->snake_size)
    {
        state->speed++;
    }
    else if (input->decrease_speed && state->speed > 0)
    {
        state->speed--;
    }
}

static void move_snake(SnakeGameState *state)
{
    const uint8_t max_x = state->field_width - state->snake_size;
    const uint8_t max_y = state->field_height - state->snake_size;

    // Store the trail at one-pixel intervals. Speed changes the number of
    // steps per update without changing the body's physical length.
    for (uint8_t step = 0; step < state->speed; step++)
    {
        SnakePoint next_head = state->snake_body[0];

        switch (state->current_direction)
        {
        case SNAKE_DIRECTION_LEFT:
            if (next_head.x == 0)
            {
                return;
            }
            next_head.x--;
            break;
        case SNAKE_DIRECTION_UP:
            if (next_head.y == 0)
            {
                return;
            }
            next_head.y--;
            break;
        case SNAKE_DIRECTION_RIGHT:
            if (next_head.x == max_x)
            {
                return;
            }
            next_head.x++;
            break;
        case SNAKE_DIRECTION_DOWN:
            if (next_head.y == max_y)
            {
                return;
            }
            next_head.y++;
            break;
        case SNAKE_DIRECTION_NONE:
        default:
            return;
        }

        for (uint16_t i = state->snake_length - 1; i > 0; i--)
        {
            state->snake_body[i] = state->snake_body[i - 1];
        }

        state->snake_body[0] = next_head;
    }
}

static void eat_fruit_if_reached(SnakeGameState *state)
{
    if (!rectangles_overlap(
            state->snake_body[0], state->snake_size,
            state->fruit_position, state->fruit_size))
    {
        return;
    }

    state->score++;

    uint16_t growth = state->snake_size;
    const SnakePoint tail = state->snake_body[state->snake_length - 1];

    while (growth-- > 0 && state->snake_length < SNAKE_MAX_TRAIL_LENGTH)
    {
        state->snake_body[state->snake_length++] = tail;
    }

    place_fruit(state);
}

bool snake_game_init(
    SnakeGameState *state,
    uint8_t field_width,
    uint8_t field_height,
    SnakeRandomSource random_source,
    void *random_context)
{
    const uint8_t object_size = 8;

    if (state == NULL || random_source == NULL ||
        field_width < object_size || field_height < object_size)
    {
        return false;
    }

    *state = (SnakeGameState){0};
    state->field_width = field_width;
    state->field_height = field_height;
    state->snake_size = object_size;
    state->fruit_size = object_size;
    state->snake_length = 1;
    state->snake_body[0] = (SnakePoint){
        .x = field_width / 2,
        .y = field_height / 2,
    };
    state->current_direction = SNAKE_DIRECTION_NONE;
    state->speed = 1;
    state->random_source = random_source;
    state->random_context = random_context;

    place_fruit(state);
    return true;
}

void snake_game_update(SnakeGameState *state, const SnakeGameInput *input)
{
    if (input->direction != SNAKE_DIRECTION_NONE &&
        (state->snake_length == 1 ||
         !directions_are_opposite(input->direction, state->current_direction)))
    {
        state->current_direction = input->direction;
    }

    update_speed(state, input);
    move_snake(state);
    eat_fruit_if_reached(state);
}
