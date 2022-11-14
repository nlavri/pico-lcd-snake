#include "game.h"

#ifndef __output_hal
#define __output_hal

/// @brief Initialize input
int init_output();

/**
    @brief Get game field width
*/
uint8_t get_field_w(void);

/**
    @brief Get game field height
*/
uint8_t get_field_h(void);

/**
    @brief Draws a game state
*/
void draw_state(GameState *const state, GameInput *const input);

#endif