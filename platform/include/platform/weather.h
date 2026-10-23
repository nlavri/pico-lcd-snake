#ifndef PICO_LCD_PLATFORM_WEATHER_H
#define PICO_LCD_PLATFORM_WEATHER_H

#include <stdbool.h>
#include <stdint.h>

#define WEATHER_LOCATION_CAPACITY 48
#define WEATHER_CONDITION_CAPACITY 32
#define WEATHER_WIND_DIRECTION_CAPACITY 4
#define WEATHER_FORECAST_DAY_CAPACITY 3

typedef enum WeatherStatus
{
    WEATHER_STATUS_UNAVAILABLE,
    WEATHER_STATUS_IDLE,
    WEATHER_STATUS_LOADING,
    WEATHER_STATUS_READY,
    WEATHER_STATUS_OFFLINE,
    WEATHER_STATUS_ERROR,
} WeatherStatus;

typedef struct WeatherForecastDay
{
    char date[11];
    char condition[WEATHER_CONDITION_CAPACITY];
    int16_t minimum_c;
    int16_t maximum_c;
    uint16_t condition_code;
    uint16_t wind_speed_kmph;
    char wind_direction[WEATHER_WIND_DIRECTION_CAPACITY];
    uint8_t rain_chance_percent;
} WeatherForecastDay;

typedef struct WeatherSnapshot
{
    WeatherStatus status;
    uint32_t revision;
    char location[WEATHER_LOCATION_CAPACITY];
    char condition[WEATHER_CONDITION_CAPACITY];
    char wind_direction[WEATHER_WIND_DIRECTION_CAPACITY];
    int16_t temperature_c;
    int16_t feels_like_c;
    uint16_t condition_code;
    uint16_t wind_speed_kmph;
    uint16_t precipitation_tenths_mm;
    uint8_t humidity_percent;
    uint8_t visibility_km;
    WeatherForecastDay forecast[WEATHER_FORECAST_DAY_CAPACITY];
    uint8_t forecast_count;
} WeatherSnapshot;

typedef struct WeatherServiceApi
{
    void (*get_snapshot)(WeatherSnapshot *snapshot);
    bool (*request_refresh)(void);
} WeatherServiceApi;

const char *weather_status_name(WeatherStatus status);

#endif
