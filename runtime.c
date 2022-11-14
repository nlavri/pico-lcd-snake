#include "runtime.h"
#include "pico/time.h"
#include <stdlib.h>

void sleep_milliseconds(uint32_t milliseconds)
{
     sleep_ms(milliseconds);
}

int get_random()
{
     return rand();
}