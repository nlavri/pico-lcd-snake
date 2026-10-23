#include <assert.h>
#include <string.h>

#include "ui.h"

static UiTextInputResult send(
    UiTextInput *input,
    InputButton button,
    InputAction action)
{
    const InputEvent event = {
        .button = button,
        .action = action,
    };
    return ui_text_input_handle_event(input, &event);
}

int main(void)
{
    UiTextInput input;
    ui_text_input_init(&input, 2, true);
    assert(input.length == 0);
    assert(input.maximum_length == 2);
    assert(input.masked);
    assert(ui_text_input_selected_character(&input) == 'q');

    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(strcmp(input.value, "q") == 0);

    assert(send(&input, INPUT_RIGHT, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(ui_text_input_selected_character(&input) == 'w');
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(strcmp(input.value, "qw") == 0);
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_UNCHANGED);

    assert(send(&input, INPUT_B, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(strcmp(input.value, "q") == 0);
    assert(send(&input, INPUT_A, INPUT_PRESSED) == UI_TEXT_INPUT_SUBMIT);
    assert(send(&input, INPUT_A, INPUT_RELEASED) ==
        UI_TEXT_INPUT_UNCHANGED);

    ui_text_input_clear(&input);
    assert(input.length == 0);
    assert(input.value[0] == '\0');
    assert(ui_text_input_selected_character(&input) == 'q');

    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(ui_text_input_selected_character(&input) == 'a');
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(ui_text_input_selected_character(&input) == '\0');
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(input.shifted);
    assert(send(&input, INPUT_UP, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(ui_text_input_selected_character(&input) == 'A');
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(strcmp(input.value, "A") == 0);
    assert(!input.shifted);
    assert(!input.shift_locked);

    ui_text_input_clear(&input);
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(input.shifted);
    assert(input.shift_locked);
    assert(send(&input, INPUT_UP, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(strcmp(input.value, "A") == 0);
    assert(input.shifted);
    assert(input.shift_locked);
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(!input.shifted);
    assert(!input.shift_locked);

    ui_text_input_clear(&input);
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_DOWN, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(ui_text_input_selected_character(&input) == '!');
    assert(send(&input, INPUT_CLICK, INPUT_PRESSED) == UI_TEXT_INPUT_CHANGED);
    assert(strcmp(input.value, "!") == 0);

    assert(ui_list_move(0, 0, 1) == 0);
    assert(ui_list_move(0, 3, -1) == 2);
    assert(ui_list_move(2, 3, 1) == 0);
    assert(ui_list_first_visible(1, 0, 3) == 0);
    assert(ui_list_first_visible(3, 0, 3) == 1);
    assert(ui_list_first_visible(0, 1, 3) == 0);

    return 0;
}
