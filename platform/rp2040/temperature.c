#include "platform/temperature.h"

#include "hardware/adc.h"

void platform_temperature_init(void)
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

float platform_temperature_read_c(void)
{
    const float conversion_factor = 3.3f / (1 << 12);
    const float adc_voltage = (float)adc_read() * conversion_factor;

    return 27.0f - (adc_voltage - 0.706f) / 0.001721f;
}
