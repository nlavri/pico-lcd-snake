#include "platform/input.h"

#include "pico/stdlib.h"

// SpotPear 1.54-inch Game LCD HAT contacts, wired to the Pico as documented
// in docs/hardware/spotpear-pico-wiring.html. The HAT's joystick contact
// orientation is reversed relative to its printed physical direction.
enum
{
    KEY_UP_PIN = 3,
    KEY_DOWN_PIN = 2,
    KEY_LEFT_PIN = 5,
    KEY_RIGHT_PIN = 4,
    KEY_CLICK_PIN = 6,
    KEY_A_PIN = 7,
    KEY_B_PIN = 8,
};

static void initialize_input_pin(uint8_t pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

static bool is_pressed(uint8_t pin)
{
    return gpio_get(pin) == 0;
}

void platform_input_init(void)
{
    initialize_input_pin(KEY_A_PIN);
    initialize_input_pin(KEY_B_PIN);
    initialize_input_pin(KEY_UP_PIN);
    initialize_input_pin(KEY_DOWN_PIN);
    initialize_input_pin(KEY_LEFT_PIN);
    initialize_input_pin(KEY_RIGHT_PIN);
    initialize_input_pin(KEY_CLICK_PIN);
}

PlatformInputState platform_input_read(void)
{
    return (PlatformInputState){
        .up = is_pressed(KEY_UP_PIN),
        .down = is_pressed(KEY_DOWN_PIN),
        .left = is_pressed(KEY_LEFT_PIN),
        .right = is_pressed(KEY_RIGHT_PIN),
        .click = is_pressed(KEY_CLICK_PIN),
        .a = is_pressed(KEY_A_PIN),
        .b = is_pressed(KEY_B_PIN),
    };
}
