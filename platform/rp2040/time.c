#include "platform/time.h"

#include <stdlib.h>

#include "pico/time.h"

void platform_sleep_ms(uint32_t milliseconds)
{
    sleep_ms(milliseconds);
}

uint32_t platform_time_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

uint32_t platform_time_us(void)
{
    return time_us_32();
}

uint32_t platform_random_u32(void)
{
    return (uint32_t)rand();
}
