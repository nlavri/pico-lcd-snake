#include <assert.h>

#include "input_events.h"

int main(void)
{
    InputEventSource source = {0};
    InputEvent events[INPUT_EVENT_BATCH_CAPACITY];
    PlatformInputState input = {0};

    assert(input_events_update(&source, &input, 1, events) == 0);

    input.a = true;
    assert(input_events_update(&source, &input, 10, events) == 0);
    assert(input_events_update(&source, &input, 35, events) == 1);
    assert(events[0].button == INPUT_A);
    assert(events[0].action == INPUT_PRESSED);

    input.a = false;
    assert(input_events_update(&source, &input, 40, events) == 0);
    assert(input_events_update(&source, &input, 65, events) == 1);
    assert(events[0].button == INPUT_A);
    assert(events[0].action == INPUT_RELEASED);

    input.a = true;
    input.b = true;
    assert(input_events_update(&source, &input, 100, events) == 0);
    assert(input_events_update(&source, &input, 125, events) == 0);
    assert(input_events_update(&source, &input, 875, events) == 1);
    assert(events[0].button == INPUT_HOME);
    assert(events[0].action == INPUT_PRESSED);

    input.a = false;
    input.b = false;
    assert(input_events_update(&source, &input, 900, events) == 0);
    assert(input_events_update(&source, &input, 925, events) == 0);

    return 0;
}
