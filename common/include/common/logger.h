#ifndef PICO_LCD_COMMON_LOGGER_H
#define PICO_LCD_COMMON_LOGGER_H

#include <stdarg.h>

typedef enum LogLevel
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
} LogLevel;

typedef void (*LogSink)(
    LogLevel level,
    const char *component,
    const char *message,
    void *context);

/*
 * The sink is called synchronously and must consume the message before it
 * returns. Passing NULL disables logging.
 */
void logger_set_sink(LogSink sink, void *context);
void logger_set_minimum_level(LogLevel level);
void logger_log(
    LogLevel level,
    const char *component,
    const char *format,
    ...);
void logger_log_v(
    LogLevel level,
    const char *component,
    const char *format,
    va_list arguments);

#define LOG_DEBUG(component, ...) \
    logger_log(LOG_LEVEL_DEBUG, component, __VA_ARGS__)
#define LOG_INFO(component, ...) \
    logger_log(LOG_LEVEL_INFO, component, __VA_ARGS__)
#define LOG_WARNING(component, ...) \
    logger_log(LOG_LEVEL_WARNING, component, __VA_ARGS__)
#define LOG_ERROR(component, ...) \
    logger_log(LOG_LEVEL_ERROR, component, __VA_ARGS__)

#endif
