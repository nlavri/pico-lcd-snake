#include "input_events.h"

#define INPUT_DEBOUNCE_MS 25
#define INPUT_HOME_HOLD_MS 750

enum
{
    INPUT_MASK_UP = 1u << 0,
    INPUT_MASK_DOWN = 1u << 1,
    INPUT_MASK_LEFT = 1u << 2,
    INPUT_MASK_RIGHT = 1u << 3,
    INPUT_MASK_CLICK = 1u << 4,
    INPUT_MASK_A = 1u << 5,
    INPUT_MASK_B = 1u << 6,
};

static uint8_t input_mask(const PlatformInputState *input)
{
    return
        (input->up ? INPUT_MASK_UP : 0) |
        (input->down ? INPUT_MASK_DOWN : 0) |
        (input->left ? INPUT_MASK_LEFT : 0) |
        (input->right ? INPUT_MASK_RIGHT : 0) |
        (input->click ? INPUT_MASK_CLICK : 0) |
        (input->a ? INPUT_MASK_A : 0) |
        (input->b ? INPUT_MASK_B : 0);
}

static size_t append_button_changes(
    uint8_t changed,
    uint8_t stable,
    uint32_t now_ms,
    InputEvent events[INPUT_EVENT_BATCH_CAPACITY])
{
    static const struct
    {
        uint8_t mask;
        InputButton button;
    } buttons[] = {
        {INPUT_MASK_UP, INPUT_UP},
        {INPUT_MASK_DOWN, INPUT_DOWN},
        {INPUT_MASK_LEFT, INPUT_LEFT},
        {INPUT_MASK_RIGHT, INPUT_RIGHT},
        {INPUT_MASK_CLICK, INPUT_CLICK},
        {INPUT_MASK_A, INPUT_A},
        {INPUT_MASK_B, INPUT_B},
    };

    size_t count = 0;
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++)
    {
        if ((changed & buttons[i].mask) == 0)
        {
            continue;
        }

        events[count++] = (InputEvent){
            .button = buttons[i].button,
            .action = (stable & buttons[i].mask)
                ? INPUT_PRESSED
                : INPUT_RELEASED,
            .timestamp_ms = now_ms,
        };
    }
    return count;
}

size_t input_events_update(
    InputEventSource *source,
    const PlatformInputState *input,
    uint32_t now_ms,
    InputEvent events[INPUT_EVENT_BATCH_CAPACITY])
{
    const uint8_t raw_mask = input_mask(input);
    if (!source->initialized)
    {
        source->candidate_mask = raw_mask;
        source->stable_mask = raw_mask;
        source->candidate_since_ms = now_ms;
        source->initialized = true;
    }

    if (raw_mask != source->candidate_mask)
    {
        source->candidate_mask = raw_mask;
        source->candidate_since_ms = now_ms;
    }

    size_t count = 0;
    if (source->candidate_mask != source->stable_mask &&
        now_ms - source->candidate_since_ms >= INPUT_DEBOUNCE_MS)
    {
        uint8_t changed = source->candidate_mask ^ source->stable_mask;
        source->stable_mask = source->candidate_mask;

        const uint8_t home_mask = INPUT_MASK_A | INPUT_MASK_B;
        if ((source->stable_mask & home_mask) == home_mask)
        {
            source->home_chord_active = true;
        }
        if (source->home_chord_active)
        {
            changed &= (uint8_t)~home_mask;
        }

        count = append_button_changes(
            changed,
            source->stable_mask,
            now_ms,
            events);

        if (source->home_chord_active &&
            (source->stable_mask & home_mask) == 0)
        {
            source->home_chord_active = false;
        }
    }

    const uint8_t home_mask = INPUT_MASK_A | INPUT_MASK_B;
    if ((source->stable_mask & home_mask) == home_mask)
    {
        if (source->home_since_ms == 0)
        {
            source->home_since_ms = now_ms;
        }
        else if (!source->home_sent &&
                 now_ms - source->home_since_ms >= INPUT_HOME_HOLD_MS &&
                 count < INPUT_EVENT_BATCH_CAPACITY)
        {
            events[count++] = (InputEvent){
                .button = INPUT_HOME,
                .action = INPUT_PRESSED,
                .timestamp_ms = now_ms,
            };
            source->home_sent = true;
        }
    }
    else
    {
        source->home_since_ms = 0;
        source->home_sent = false;
    }

    return count;
}
