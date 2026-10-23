#include <assert.h>
#include <string.h>

#include "common/logger.h"

typedef struct CapturedLog
{
    unsigned count;
    LogLevel level;
    const char *component;
    char message[64];
} CapturedLog;

static void capture_sink(
    LogLevel level,
    const char *component,
    const char *message,
    void *context)
{
    CapturedLog *captured = context;
    captured->count++;
    captured->level = level;
    captured->component = component;
    strncpy(captured->message, message, sizeof(captured->message) - 1);
    captured->message[sizeof(captured->message) - 1] = '\0';
}

int main(void)
{
    CapturedLog first = {0};
    CapturedLog replacement = {0};

    logger_set_sink(capture_sink, &first);
    logger_set_minimum_level(LOG_LEVEL_INFO);

    LOG_DEBUG("test", "not emitted");
    assert(first.count == 0);

    LOG_INFO("test", "value=%u", 42u);
    assert(first.count == 1);
    assert(first.level == LOG_LEVEL_INFO);
    assert(strcmp(first.component, "test") == 0);
    assert(strcmp(first.message, "value=42") == 0);

    logger_set_sink(capture_sink, &replacement);
    LOG_ERROR("other", "replacement sink");
    assert(first.count == 1);
    assert(replacement.count == 1);
    assert(replacement.level == LOG_LEVEL_ERROR);
    assert(strcmp(replacement.component, "other") == 0);
    assert(strcmp(replacement.message, "replacement sink") == 0);

    logger_set_sink(NULL, NULL);
    LOG_ERROR("test", "disabled");
    assert(replacement.count == 1);

    return 0;
}
