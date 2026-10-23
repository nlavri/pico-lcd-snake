#include "platform/log.h"

#include <stdio.h>

#include "common/logger.h"

static const char *level_name(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_WARNING:
        return "WARN";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_INFO:
    default:
        return "INFO";
    }
}

static void stdio_sink(
    LogLevel level,
    const char *component,
    const char *message,
    void *context)
{
    (void)context;
    printf(
        "[%s] [%s] %s\r\n",
        level_name(level),
        component == NULL ? "general" : component,
        message);
}

void platform_log_init(void)
{
    logger_set_sink(stdio_sink, NULL);
}
