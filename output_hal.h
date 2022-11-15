#include "game.h"

#ifndef __output_hal
#define __output_hal

#define BORDER_WIDTH 2
#define STATUS_BAR_HEIGHT 15

/// @struct OutputInitResult
///  @brief Output Initialization Result
///
typedef struct OutputInitResult
{
    bool ok;
    uint8_t w;
    uint8_t h;
} OutputInitResult;

/// @brief Initialize input
OutputInitResult init_output();

/**
    @brief Draws a game state
*/
void draw_state(GameState *const state, GameInput *const input);

#endif