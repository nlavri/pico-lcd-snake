#include "common/logger.h"

#include <stddef.h>
#include <stdio.h>

#define LOGGER_MESSAGE_CAPACITY 192

static LogSink active_sink;
static void *active_sink_context;
static LogLevel minimum_level = LOG_LEVEL_INFO;

void logger_set_sink(LogSink sink, void *context)
{
    active_sink = sink;
    active_sink_context = context;
}

void logger_set_minimum_level(LogLevel level)
{
    minimum_level = level;
}

void logger_log_v(
    LogLevel level,
    const char *component,
    const char *format,
    va_list arguments)
{
    if (active_sink == NULL || level < minimum_level || format == NULL)
    {
        return;
    }

    char message[LOGGER_MESSAGE_CAPACITY];
    vsnprintf(message, sizeof(message), format, arguments);
    active_sink(level, component, message, active_sink_context);
}

void logger_log(
    LogLevel level,
    const char *component,
    const char *format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);
    logger_log_v(level, component, format, arguments);
    va_end(arguments);
}
