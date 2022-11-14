#include "game.h"
#include "stdbool.h"

#ifndef __input_hal
#define __input_hal

void init_input();

/**
    @brief Gets a direction input from a user
*/
MOV_DIR get_direction_input();

/**
    @brief Gets a control button state. Non-zero means active.
*/
bool get_control_button_state();

#endif
