#include "input_hal.h"

#include "pico/stdlib.h"

//LOGICAL KEYS TO PINS MAPPING
static const uint8_t KEY_A = 15; 
static const uint8_t KEY_B = 17; 

static const uint8_t KEY_UP = 2;
static const uint8_t KEY_DOWN = 18;
static const uint8_t KEY_LEFT = 16;
static const uint8_t KEY_RIGHT = 20;
static const uint8_t KEY_CTRL = 3;

void init_input_pin(uint8_t pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void init_input()
{
    init_input_pin(KEY_A);    
    init_input_pin(KEY_B);
		 
	init_input_pin(KEY_UP);
    init_input_pin(KEY_DOWN);
    init_input_pin(KEY_LEFT);
    init_input_pin(KEY_RIGHT);
    init_input_pin(KEY_CTRL);
}

/**
    @brief Gets a direction input from a user
*/
MOV_DIR get_direction_input()
{
    if(gpio_get(KEY_UP) == 0)
        return MOV_DIR_UP;

    if(gpio_get(KEY_DOWN) == 0)
        return MOV_DIR_DOWN;

    if(gpio_get(KEY_LEFT) == 0)
        return MOV_DIR_LEFT;

    if(gpio_get(KEY_RIGHT) == 0)
        return MOV_DIR_RIGHT;

    return MOV_DIR_UNDEFINED;
}

/**
    @brief Gets a control button state
*/
bool get_control_button_state()
{
    return gpio_get(KEY_CTRL) == 0;
}


/**
    @brief Gets "A" button state
*/
bool get_key_a_state()
{
    return gpio_get(KEY_A) == 0;
}


/**
    @brief Gets "B" button state
*/
bool get_key_b_state()
{
    return gpio_get(KEY_B) == 0;
}