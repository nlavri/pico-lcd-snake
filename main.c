#include <stdio.h>
#include "pico/stdlib.h"

#include "game.h"
#include "input_hal.h"
#include "output_hal.h"

int main()
{
    stdio_init_all();

    GameState state;
    GameInput input;

    int init_result = init_game(&state, &input);
    if (init_result != 0)
    {
        printf("init_game failed %d", init_result);
        return init_result;
    }

    printf("game initialized");

    main_loop(&state, &input);

    return 0;
}
