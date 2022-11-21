#include "stdint.h"
#include "stdbool.h"

#ifndef __game_h
#define __game_h

/// @struct Movement_Direction
/// @brief Movement direction
///
typedef enum MOV_DIR
{
  MOV_DIR_UNDEFINED = 0,
  MOV_DIR_LEFT = 1,
  MOV_DIR_UP = 2,
  MOV_DIR_RIGHT = 3,
  MOV_DIR_DOWN = 4
} MOV_DIR;

/// @struct Point
///  @brief 2-D Point
///
typedef struct Point
{
  uint8_t x;
  uint8_t y;
} Point;

/// @struct GameState
///  @brief Main Game State
///
typedef struct GameState
{
  bool exit_requested;

  uint32_t is_paused;

  uint32_t score;

  uint8_t field_w;
  uint8_t field_h;

  /// @brief Top-Left snake position
  Point snake_position;

  /// @brief snake size (snake is square)
  uint8_t snake_size;

  Point prev_snake_position;
  MOV_DIR current_direction;

  /// @brief Top-Left fruit position
  Point fruit_position;

  /// @brief Fruit size (fruit is square)
  uint8_t fruit_size;

  Point prev_fruit_position;

  uint8_t speed;

} GameState;

/** @struct GameInput
 *  @brief Game Input
 */
typedef struct GameInput
{
  MOV_DIR direction;

  bool btn_ctrl;

  bool btn_a;

  bool btn_b;

} GameInput;

int init_game(GameState *const state, GameInput *const input);

void main_loop(GameState *const state, GameInput *const input);

#endif