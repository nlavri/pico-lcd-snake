#include "platform/weather.h"

const char *weather_status_name(WeatherStatus status)
{
    switch (status)
    {
        case WEATHER_STATUS_IDLE:
            return "Ready to refresh";
        case WEATHER_STATUS_LOADING:
            return "Loading";
        case WEATHER_STATUS_READY:
            return "Updated";
        case WEATHER_STATUS_OFFLINE:
            return "Wi-Fi offline";
        case WEATHER_STATUS_UNAVAILABLE:
            return "Unavailable";
        case WEATHER_STATUS_ERROR:
        default:
            return "NO INTERNET";
    }
}
