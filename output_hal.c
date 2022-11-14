#include "output_hal.h"

#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in14.h"

int init_output()
{
    DEV_Delay_ms(100);

    if (DEV_Module_Init() != 0)
    {
        return -1;
    }

    DEV_SET_PWM(50);

    LCD_1IN14_Init(HORIZONTAL);
    LCD_1IN14_Clear(WHITE);

    return 0;
}

/**
    @brief Draws a game state
*/
void draw_state(GameState *const state, GameInput *const input)
{
    printf("Current direction: %d \r\n", state->current_direction);
    printf("Control button: %d \r\n", input->control_button);
}

/**
    @brief Get game field width
*/
uint8_t get_field_w(void)
{
    return 40;
}

/**
    @brief Get game field height
*/
uint8_t get_field_h(void)
{
    return 40;
}