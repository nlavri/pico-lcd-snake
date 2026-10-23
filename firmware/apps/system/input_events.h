#ifndef PICO_LCD_SYSTEM_INPUT_EVENTS_H
#define PICO_LCD_SYSTEM_INPUT_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"
#include "platform/input.h"

#define INPUT_EVENT_BATCH_CAPACITY 8

typedef struct InputEventSource
{
    uint8_t candidate_mask;
    uint8_t stable_mask;
    uint32_t candidate_since_ms;
    uint32_t home_since_ms;
    bool home_chord_active;
    bool home_sent;
    bool initialized;
} InputEventSource;

size_t input_events_update(
    InputEventSource *source,
    const PlatformInputState *input,
    uint32_t now_ms,
    InputEvent events[INPUT_EVENT_BATCH_CAPACITY]);

#endif
