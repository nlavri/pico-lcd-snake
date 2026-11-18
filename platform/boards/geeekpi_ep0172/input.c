#include "platform/input.h"

#include "touch.h"

#include "common/logger.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

enum
{
    JOYSTICK_X_PIN = 26,
    JOYSTICK_Y_PIN = 27,
    JOYSTICK_X_INPUT = 0,
    JOYSTICK_Y_INPUT = 1,
    BUTTON_SELECT_PIN = 15,
    BUTTON_BACK_PIN = 14,
    JOYSTICK_LOW_THRESHOLD = 1200,
    JOYSTICK_HIGH_THRESHOLD = 2900,
};

static bool idle_state_reported;

static void initialize_button(uint8_t pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

static uint16_t read_adc(uint8_t input)
{
    adc_select_input(input);
    return adc_read();
}

void platform_input_init(void)
{
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
    initialize_button(BUTTON_SELECT_PIN);
    initialize_button(BUTTON_BACK_PIN);
    ep0172_touch_init();
}

PlatformInputState platform_input_read(void)
{
    const uint16_t x = read_adc(JOYSTICK_X_INPUT);
    const uint16_t y = read_adc(JOYSTICK_Y_INPUT);
    const bool select_level = gpio_get(BUTTON_SELECT_PIN);
    const bool back_level = gpio_get(BUTTON_BACK_PIN);
    uint16_t touch_x = 0;
    uint16_t touch_y = 0;
    const bool touch = ep0172_touch_read(&touch_x, &touch_y);
    if (!idle_state_reported)
    {
        LOG_INFO(
            "input",
            "EP-0172 raw idle: x=%u y=%u BTN1=%u BTN2=%u",
            x,
            y,
            select_level ? 1u : 0u,
            back_level ? 1u : 0u);
        idle_state_reported = true;
    }
    return (PlatformInputState){
        .up = y > JOYSTICK_HIGH_THRESHOLD,
        .down = y < JOYSTICK_LOW_THRESHOLD,
        .left = x < JOYSTICK_LOW_THRESHOLD,
        .right = x > JOYSTICK_HIGH_THRESHOLD,
        .click = !select_level,
        .a = false,
        .b = !back_level,
        .touch = touch,
        .touch_x = touch_x,
        .touch_y = touch_y,
    };
}
