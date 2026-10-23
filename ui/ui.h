#ifndef PICO_LCD_UI_H
#define PICO_LCD_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"

#define UI_TEXT_INPUT_CAPACITY 65
#define UI_BRIGHTNESS_MINIMUM 10
#define UI_BRIGHTNESS_MAXIMUM 100
#define UI_BRIGHTNESS_STEP 10

typedef enum UiAccent
{
    UI_ACCENT_VIOLET,
    UI_ACCENT_BLUE,
    UI_ACCENT_SKY,
    UI_ACCENT_MINT,
    UI_ACCENT_TERMINAL_GREEN,
    UI_ACCENT_AMBER,
    UI_ACCENT_ORANGE,
    UI_ACCENT_ORANGE_RED,
    UI_ACCENT_DARK_RED,
    UI_ACCENT_COUNT,
} UiAccent;

typedef enum UiTextInputResult
{
    UI_TEXT_INPUT_UNCHANGED,
    UI_TEXT_INPUT_CHANGED,
    UI_TEXT_INPUT_SUBMIT,
} UiTextInputResult;

typedef struct UiTextInput
{
    char value[UI_TEXT_INPUT_CAPACITY];
    size_t length;
    size_t maximum_length;
    uint8_t keyboard_page;
    uint8_t selected_key;
    bool masked;
    bool shifted;
    bool shift_locked;
} UiTextInput;

void ui_theme_init(void);
UiAccent ui_accent(void);
const char *ui_accent_name(UiAccent accent);
DisplayColor ui_accent_color(void);
DisplayColor ui_accent_palette_color(UiAccent accent);
DisplayColor ui_chrome_color(void);
DisplayColor ui_secondary_text_color(void);
DisplayColor ui_primary_text_color(void);
DisplayColor ui_neutral_text_color(void);
DisplayColor ui_menu_text_color(void);
DisplayColor ui_success_color(void);
DisplayColor ui_activity_color(void);
DisplayColor ui_error_color(void);
void ui_preview_accent(UiAccent accent);
bool ui_commit_accent(UiAccent accent);
uint8_t ui_brightness(void);
void ui_preview_brightness(uint8_t percent);
bool ui_commit_brightness(uint8_t percent);

void ui_text_input_init(
    UiTextInput *input,
    size_t maximum_length,
    bool masked);
void ui_text_input_clear(UiTextInput *input);
UiTextInputResult ui_text_input_handle_event(
    UiTextInput *input,
    const InputEvent *event);
char ui_text_input_selected_character(const UiTextInput *input);

size_t ui_list_move(size_t selected, size_t item_count, int direction);
size_t ui_list_first_visible(
    size_t selected,
    size_t current_first,
    size_t visible_count);

void ui_draw_header(
    DisplaySurface *surface,
    const char *title);
void ui_draw_header_with_detail(
    DisplaySurface *surface,
    const char *title,
    const char *detail);
void ui_draw_footer(DisplaySurface *surface, const char *legend);
void ui_draw_list_item(
    DisplaySurface *surface,
    uint16_t y,
    const char *label,
    const char *detail,
    bool selected);
void ui_draw_menu_item(
    DisplaySurface *surface,
    uint16_t y,
    const char *label,
    const char *detail,
    bool selected,
    bool enabled);
void ui_draw_text_input(
    DisplaySurface *surface,
    const UiTextInput *input,
    uint16_t y);
void ui_draw_virtual_keyboard(
    DisplaySurface *surface,
    const UiTextInput *input,
    uint16_t y);

#endif
