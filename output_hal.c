#include "output_hal.h"

#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in14.h"

uint16_t *default_image;
char *status_bar;
OutputInitResult init_output()
{
    if (DEV_Module_Init() != 0)
    {
        return (OutputInitResult){.ok = false};
    }

    DEV_SET_PWM(50);

    LCD_1IN14_Init(HORIZONTAL);
    LCD_1IN14_Clear(GRAY);

    // LCD_1IN14 filled in LCD_1IN14_Init()
    uint16_t image_size = LCD_1IN14.WIDTH * LCD_1IN14.HEIGHT * 2;
    if ((default_image = (uint16_t *)malloc(image_size)) == NULL)
    {
        printf("Cannot malloc image memory\r\n");
        return (OutputInitResult){.ok = false};
    }

    Paint_NewImage((UBYTE *)default_image, LCD_1IN14.WIDTH, LCD_1IN14.HEIGHT, 0, GRAY);
    Paint_SetScale(65);
    Paint_Clear(GRAY);
    Paint_SetRotate(ROTATE_0);
    Paint_Clear(GRAY);

    Paint_DrawRectangle(
        BORDER_WIDTH,
        STATUS_BAR_HEIGHT,
        LCD_1IN14.WIDTH - 1,
        LCD_1IN14.HEIGHT,
        BLUE, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);

    // Paint_DrawRectangle(
    //     20,
    //     STATUS_BAR_HEIGHT + 20,
    //     40,
    //     STATUS_BAR_HEIGHT + 40,
    //     BLUE, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);

    LCD_1IN14_Display(default_image);

    DEV_Delay_ms(100);

    status_bar = (char *)malloc(12 * sizeof(char));

    printf("Initialized");

    return (OutputInitResult){
        .ok = true,
        .w = LCD_1IN14.WIDTH - (2 * BORDER_WIDTH) - DOT_PIXEL_8X8,
        .h = LCD_1IN14.HEIGHT - STATUS_BAR_HEIGHT - (2 * BORDER_WIDTH) - DOT_PIXEL_8X8};
}

/**
    @brief Draws a game state
*/
void draw_state(GameState *const state, GameInput *const input)
{
    printf("Current direction: %d \r\n", state->current_direction);
    printf("Control button: %d \r\n", input->btn_ctrl);
    printf("W: %d, H: %d \r\n", state->field_w, state->field_h);
    printf("SX: %d, SY: %d \r\n", state->snake_position.x, state->snake_position.y);
    printf("PSX: %d, PSY: %d \r\n", state->prev_snake_position.x, state->prev_snake_position.y);

    sprintf(status_bar, "Score: %d", state->score);
    Paint_DrawString_EN(1, 1, status_bar, &Font12, GRAY, WHITE);

    Paint_DrawPoint(
        BORDER_WIDTH + BORDER_WIDTH + state->prev_snake_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->prev_snake_position.y,
        GRAY, DOT_PIXEL_8X8, DOT_FILL_RIGHTUP);

    Paint_DrawPoint(
        BORDER_WIDTH + BORDER_WIDTH + state->snake_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->snake_position.y,
        YELLOW, DOT_PIXEL_8X8, DOT_FILL_RIGHTUP);

    Paint_DrawPoint(
        BORDER_WIDTH + BORDER_WIDTH + state->prev_fruit_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->prev_fruit_position.y,
        GRAY, DOT_PIXEL_8X8, DOT_FILL_RIGHTUP);

    Paint_DrawPoint(
        BORDER_WIDTH + BORDER_WIDTH + state->fruit_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->fruit_position.y,
        GREEN, DOT_PIXEL_8X8, DOT_FILL_RIGHTUP);

    LCD_1IN14_Display(default_image);
}
