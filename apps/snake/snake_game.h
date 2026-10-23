#ifndef PICO_LCD_SNAKE_GAME_H
#define PICO_LCD_SNAKE_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SNAKE_MAX_TRAIL_LENGTH 128

typedef enum SnakeDirection
{
    SNAKE_DIRECTION_NONE = 0,
    SNAKE_DIRECTION_LEFT,
    SNAKE_DIRECTION_UP,
    SNAKE_DIRECTION_RIGHT,
    SNAKE_DIRECTION_DOWN
} SnakeDirection;

typedef struct SnakePoint
{
    uint8_t x;
    uint8_t y;
} SnakePoint;

typedef uint32_t (*SnakeRandomSource)(void *context);

typedef struct SnakeGameInput
{
    SnakeDirection direction;
    bool increase_speed;
    bool decrease_speed;
} SnakeGameInput;

typedef struct SnakeGameState
{
    uint32_t score;
    uint8_t field_width;
    uint8_t field_height;

    uint8_t snake_size;
    SnakePoint snake_body[SNAKE_MAX_TRAIL_LENGTH];
    uint16_t snake_length;
    SnakeDirection current_direction;

    SnakePoint fruit_position;
    uint8_t fruit_size;

    uint8_t speed;

    SnakeRandomSource random_source;
    void *random_context;
} SnakeGameState;

bool snake_game_init(
    SnakeGameState *state,
    uint8_t field_width,
    uint8_t field_height,
    SnakeRandomSource random_source,
    void *random_context);

void snake_game_update(SnakeGameState *state, const SnakeGameInput *input);

#endif
