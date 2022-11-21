#include "output_hal.h"

#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in14.h"

#include "hardware/adc.h"

uint16_t *default_image;
char *status_bar;
uint8_t snake_size;
uint8_t fruit_size;

// pixels start from 1
#define left_shift 2
#define top_shift 1

DOT_PIXEL to_pixel_size(uint8_t obj_size)
{
    return (DOT_PIXEL)obj_size;
}

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
        left_shift,
        STATUS_BAR_HEIGHT,
        LCD_1IN14.WIDTH - 1,
        LCD_1IN14.HEIGHT - 1,
        BLUE, BORDER_WIDTH, DRAW_FILL_EMPTY);

    LCD_1IN14_Display(default_image);

    DEV_Delay_ms(100);

    status_bar = (char *)malloc(30 * sizeof(char));

    printf("Initialized");

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    return (OutputInitResult){
        .ok = true,
        .w = LCD_1IN14.WIDTH - (2 * BORDER_WIDTH) - left_shift,
        .h = LCD_1IN14.HEIGHT - STATUS_BAR_HEIGHT - (2 * BORDER_WIDTH)};
}

void post_init_output(GameState *const state)
{
    snake_size = to_pixel_size(state->snake_size);
    fruit_size = to_pixel_size(state->fruit_size);
}


/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
float read_onboard_temperature()
{
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    return tempC;
}


/**
    @brief Draws a game state
*/
void draw_state(GameState *const state, GameInput *const input)
{
    printf("W: %d H: %d\r\n", state->field_w, state->field_h);
    printf("SX: %d SY: %d\r\n", state->snake_position.x, state->snake_position.y);
    printf("SPEED: %d\r\n", state->speed);

    float temperature = read_onboard_temperature();
    sprintf(status_bar, "Score:%2d | Speed:%2d | Temp: %.0fC", state->score, state->speed, temperature);
    Paint_DrawString_EN(left_shift, top_shift, status_bar, &Font12, GRAY, WHITE);

    // Erase prev snake sprite
    Paint_DrawPoint(
        left_shift + BORDER_WIDTH + state->prev_snake_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->prev_snake_position.y,
        GRAY, snake_size, DOT_FILL_RIGHTUP);

    // Erase prev fruit sprite
    Paint_DrawPoint(
        left_shift + BORDER_WIDTH + state->prev_fruit_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->prev_fruit_position.y,
        GRAY, fruit_size, DOT_FILL_RIGHTUP);

    // Draw current snake sprite
    Paint_DrawPoint(
        left_shift + BORDER_WIDTH + state->snake_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->snake_position.y,
        YELLOW, snake_size, DOT_FILL_RIGHTUP);

    // Draw current fruit sprite
    Paint_DrawPoint(
        left_shift + BORDER_WIDTH + state->fruit_position.x,
        STATUS_BAR_HEIGHT + BORDER_WIDTH + state->fruit_position.y,
        GREEN, fruit_size, DOT_FILL_RIGHTUP);

    LCD_1IN14_Display(default_image);
}
