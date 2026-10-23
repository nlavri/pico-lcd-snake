#ifndef PICO_LCD_NETWORK_SERVICE_H
#define PICO_LCD_NETWORK_SERVICE_H

#include <stdbool.h>

#include "platform/network.h"
#include "platform/weather.h"

bool network_service_start(void);
const NetworkServiceApi *network_service_api(void);
const WeatherServiceApi *weather_service_api(void);

#endif
