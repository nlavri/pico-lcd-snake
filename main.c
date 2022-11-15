#define DEBUG

#include <stdio.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "game.h"
#include "input_hal.h"
#include "output_hal.h"

int reset_boot()
{
    reset_usb_boot(0, 0);
    return 0;
}

int main()
{
    stdio_init_all();

    GameState state;
    GameInput input;

    int init_result = init_game(&state, &input);
    if (init_result != 0)
    {
        printf("init_game failed %d \r\n", init_result);
        return reset_boot();
    }

    main_loop(&state, &input);
    return reset_boot();
}
