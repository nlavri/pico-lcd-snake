#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "platform/preferences.h"

#define UI_RGB565(red, green, blue) ((DisplayColor)( \
    (((uint16_t)(red) & 0xf8u) << 8) | \
    (((uint16_t)(green) & 0xfcu) << 3) | \
    ((uint16_t)(blue) >> 3)))

typedef struct UiAccentDefinition
{
    const char *name;
    DisplayColor color;
} UiAccentDefinition;

static const UiAccentDefinition accent_palette[UI_ACCENT_COUNT] = {
    [UI_ACCENT_VIOLET] = {"Violet", UI_RGB565(155, 93, 229)},
    [UI_ACCENT_BLUE] = {"Blue", UI_RGB565(72, 149, 239)},
    [UI_ACCENT_SKY] = {"Sky", UI_RGB565(76, 201, 240)},
    [UI_ACCENT_MINT] = {"Mint", UI_RGB565(79, 209, 161)},
    [UI_ACCENT_TERMINAL_GREEN] = {"Terminal Green", UI_RGB565(36, 155, 88)},
    [UI_ACCENT_AMBER] = {"Amber", UI_RGB565(246, 189, 96)},
    [UI_ACCENT_ORANGE] = {"Orange", UI_RGB565(255, 159, 28)},
    [UI_ACCENT_ORANGE_RED] = {"Orange Red", UI_RGB565(255, 90, 54)},
    [UI_ACCENT_DARK_RED] = {"Dark Red", UI_RGB565(184, 58, 58)},
};

static UiAccent current_accent = UI_ACCENT_MINT;
static uint8_t current_brightness = 50;

static uint8_t rgb565_red(DisplayColor color)
{
    return (uint8_t)(((color >> 11) & 0x1fu) * 255u / 31u);
}

static uint8_t rgb565_green(DisplayColor color)
{
    return (uint8_t)(((color >> 5) & 0x3fu) * 255u / 63u);
}

static uint8_t rgb565_blue(DisplayColor color)
{
    return (uint8_t)((color & 0x1fu) * 255u / 31u);
}

static uint8_t mix_channel_with_white(uint8_t channel)
{
    return (uint8_t)(((uint16_t)channel * 55u + 255u * 45u) / 100u);
}

void ui_theme_init(void)
{
    PlatformPreferences preferences;
    if (platform_preferences_load(&preferences))
    {
        if (preferences.accent_index < UI_ACCENT_COUNT)
        {
            current_accent = (UiAccent)preferences.accent_index;
        }
        if (preferences.brightness_percent >= UI_BRIGHTNESS_MINIMUM &&
            preferences.brightness_percent <= UI_BRIGHTNESS_MAXIMUM)
        {
            current_brightness = preferences.brightness_percent;
        }
    }
    display_set_brightness(current_brightness);
}

UiAccent ui_accent(void)
{
    return current_accent;
}

const char *ui_accent_name(UiAccent accent)
{
    return accent < UI_ACCENT_COUNT
        ? accent_palette[accent].name
        : accent_palette[UI_ACCENT_MINT].name;
}

DisplayColor ui_accent_palette_color(UiAccent accent)
{
    return accent < UI_ACCENT_COUNT
        ? accent_palette[accent].color
        : accent_palette[UI_ACCENT_MINT].color;
}

DisplayColor ui_accent_color(void)
{
    return ui_accent_palette_color(current_accent);
}

DisplayColor ui_chrome_color(void)
{
    return UI_RGB565(112, 129, 138);
}

DisplayColor ui_secondary_text_color(void)
{
    return UI_RGB565(166, 184, 191);
}

DisplayColor ui_primary_text_color(void)
{
    const DisplayColor accent = ui_accent_color();
    return UI_RGB565(
        mix_channel_with_white(rgb565_red(accent)),
        mix_channel_with_white(rgb565_green(accent)),
        mix_channel_with_white(rgb565_blue(accent)));
}

DisplayColor ui_neutral_text_color(void)
{
    return DISPLAY_COLOR_WHITE;
}

DisplayColor ui_menu_text_color(void)
{
    return ui_primary_text_color();
}

DisplayColor ui_success_color(void)
{
    return UI_RGB565(90, 201, 139);
}

DisplayColor ui_activity_color(void)
{
    return UI_RGB565(98, 168, 245);
}

DisplayColor ui_error_color(void)
{
    return UI_RGB565(224, 108, 117);
}

void ui_preview_accent(UiAccent accent)
{
    if (accent < UI_ACCENT_COUNT)
    {
        current_accent = accent;
    }
}

bool ui_commit_accent(UiAccent accent)
{
    if (accent >= UI_ACCENT_COUNT)
    {
        return false;
    }
    current_accent = accent;
    const PlatformPreferences preferences = {
        .accent_index = (uint8_t)accent,
        .brightness_percent = current_brightness,
    };
    return platform_preferences_save(&preferences);
}

uint8_t ui_brightness(void)
{
    return current_brightness;
}

void ui_preview_brightness(uint8_t percent)
{
    if (percent < UI_BRIGHTNESS_MINIMUM)
    {
        percent = UI_BRIGHTNESS_MINIMUM;
    }
    if (percent > UI_BRIGHTNESS_MAXIMUM)
    {
        percent = UI_BRIGHTNESS_MAXIMUM;
    }
    current_brightness = percent;
    display_set_brightness(current_brightness);
}

bool ui_commit_brightness(uint8_t percent)
{
    ui_preview_brightness(percent);
    const PlatformPreferences preferences = {
        .accent_index = (uint8_t)current_accent,
        .brightness_percent = current_brightness,
    };
    return platform_preferences_save(&preferences);
}

typedef enum UiKeyboardKeyType
{
    UI_KEY_CHARACTER,
    UI_KEY_SHIFT,
    UI_KEY_SYMBOLS,
    UI_KEY_LETTERS,
    UI_KEY_SPACE,
} UiKeyboardKeyType;

typedef struct UiKeyboardKey
{
    UiKeyboardKeyType type;
    char character;
    uint8_t row;
    uint8_t column;
    uint8_t span;
} UiKeyboardKey;

#define CHARACTER_KEY(value, key_row, key_column) \
    {UI_KEY_CHARACTER, value, key_row, key_column, 1}
#define ACTION_KEY(kind, key_row, key_column, key_span) \
    {kind, '\0', key_row, key_column, key_span}

static const UiKeyboardKey main_keys[] = {
    CHARACTER_KEY('1', 0, 0), CHARACTER_KEY('2', 0, 1),
    CHARACTER_KEY('3', 0, 2), CHARACTER_KEY('4', 0, 3),
    CHARACTER_KEY('5', 0, 4), CHARACTER_KEY('6', 0, 5),
    CHARACTER_KEY('7', 0, 6), CHARACTER_KEY('8', 0, 7),
    CHARACTER_KEY('9', 0, 8), CHARACTER_KEY('0', 0, 9),
    CHARACTER_KEY('q', 1, 0), CHARACTER_KEY('w', 1, 1),
    CHARACTER_KEY('e', 1, 2), CHARACTER_KEY('r', 1, 3),
    CHARACTER_KEY('t', 1, 4), CHARACTER_KEY('y', 1, 5),
    CHARACTER_KEY('u', 1, 6), CHARACTER_KEY('i', 1, 7),
    CHARACTER_KEY('o', 1, 8), CHARACTER_KEY('p', 1, 9),
    CHARACTER_KEY('a', 2, 1), CHARACTER_KEY('s', 2, 2),
    CHARACTER_KEY('d', 2, 3), CHARACTER_KEY('f', 2, 4),
    CHARACTER_KEY('g', 2, 5), CHARACTER_KEY('h', 2, 6),
    CHARACTER_KEY('j', 2, 7), CHARACTER_KEY('k', 2, 8),
    CHARACTER_KEY('l', 2, 9),
    ACTION_KEY(UI_KEY_SHIFT, 3, 0, 2),
    CHARACTER_KEY('z', 3, 2), CHARACTER_KEY('x', 3, 3),
    CHARACTER_KEY('c', 3, 4), CHARACTER_KEY('v', 3, 5),
    CHARACTER_KEY('b', 3, 6), CHARACTER_KEY('n', 3, 7),
    CHARACTER_KEY('m', 3, 8),
    ACTION_KEY(UI_KEY_SYMBOLS, 4, 0, 3),
    ACTION_KEY(UI_KEY_SPACE, 4, 3, 7),
};

static const UiKeyboardKey symbol_keys[] = {
    CHARACTER_KEY('!', 0, 0), CHARACTER_KEY('@', 0, 1),
    CHARACTER_KEY('#', 0, 2), CHARACTER_KEY('$', 0, 3),
    CHARACTER_KEY('%', 0, 4), CHARACTER_KEY('^', 0, 5),
    CHARACTER_KEY('&', 0, 6), CHARACTER_KEY('*', 0, 7),
    CHARACTER_KEY('(', 0, 8), CHARACTER_KEY(')', 0, 9),
    CHARACTER_KEY('-', 1, 0), CHARACTER_KEY('_', 1, 1),
    CHARACTER_KEY('=', 1, 2), CHARACTER_KEY('+', 1, 3),
    CHARACTER_KEY('[', 1, 4), CHARACTER_KEY(']', 1, 5),
    CHARACTER_KEY('{', 1, 6), CHARACTER_KEY('}', 1, 7),
    CHARACTER_KEY('\\', 1, 8), CHARACTER_KEY('|', 1, 9),
    CHARACTER_KEY(';', 2, 0), CHARACTER_KEY(':', 2, 1),
    CHARACTER_KEY('\'', 2, 2), CHARACTER_KEY('"', 2, 3),
    CHARACTER_KEY(',', 2, 4), CHARACTER_KEY('.', 2, 5),
    CHARACTER_KEY('<', 2, 6), CHARACTER_KEY('>', 2, 7),
    CHARACTER_KEY('/', 2, 8), CHARACTER_KEY('?', 2, 9),
    CHARACTER_KEY('`', 3, 4), CHARACTER_KEY('~', 3, 5),
    ACTION_KEY(UI_KEY_LETTERS, 4, 0, 3),
    ACTION_KEY(UI_KEY_SPACE, 4, 3, 7),
};

enum
{
    UI_KEYBOARD_MAIN,
    UI_KEYBOARD_SYMBOLS,
    UI_KEYBOARD_INITIAL_KEY = 10,
};

static const UiKeyboardKey *keyboard_keys(
    const UiTextInput *input,
    size_t *count)
{
    if (input->keyboard_page == UI_KEYBOARD_SYMBOLS)
    {
        *count = sizeof(symbol_keys) / sizeof(symbol_keys[0]);
        return symbol_keys;
    }
    *count = sizeof(main_keys) / sizeof(main_keys[0]);
    return main_keys;
}

static const UiKeyboardKey *selected_key(const UiTextInput *input)
{
    size_t count;
    const UiKeyboardKey *keys = keyboard_keys(input, &count);
    return &keys[input->selected_key < count ? input->selected_key : 0];
}

void ui_text_input_init(
    UiTextInput *input,
    size_t maximum_length,
    bool masked)
{
    *input = (UiTextInput){
        .maximum_length = maximum_length < UI_TEXT_INPUT_CAPACITY
            ? maximum_length
            : UI_TEXT_INPUT_CAPACITY - 1,
        .selected_key = UI_KEYBOARD_INITIAL_KEY,
        .masked = masked,
    };
}

void ui_text_input_clear(UiTextInput *input)
{
    memset(input->value, 0, sizeof(input->value));
    input->length = 0;
    input->keyboard_page = UI_KEYBOARD_MAIN;
    input->selected_key = UI_KEYBOARD_INITIAL_KEY;
    input->shifted = false;
    input->shift_locked = false;
}

static int key_center_twice(const UiKeyboardKey *key)
{
    return key->column * 2 + key->span - 1;
}

static void move_horizontal(UiTextInput *input, int direction)
{
    size_t count;
    const UiKeyboardKey *keys = keyboard_keys(input, &count);
    const UiKeyboardKey *current = selected_key(input);
    size_t candidate = input->selected_key;
    for (size_t i = 0; i < count; i++)
    {
        if (keys[i].row != current->row)
        {
            continue;
        }
        if (direction < 0 && keys[i].column < current->column &&
            (candidate == input->selected_key ||
             keys[i].column > keys[candidate].column))
        {
            candidate = i;
        }
        if (direction > 0 && keys[i].column > current->column &&
            (candidate == input->selected_key ||
             keys[i].column < keys[candidate].column))
        {
            candidate = i;
        }
    }
    input->selected_key = (uint8_t)candidate;
}

static void move_vertical(UiTextInput *input, int direction)
{
    size_t count;
    const UiKeyboardKey *keys = keyboard_keys(input, &count);
    const UiKeyboardKey *current = selected_key(input);
    const int target_row = (int)current->row + direction;
    if (target_row < 0 || target_row >= 5)
    {
        return;
    }

    size_t candidate = input->selected_key;
    int nearest_distance = 100;
    const int current_center = key_center_twice(current);
    for (size_t i = 0; i < count; i++)
    {
        if (keys[i].row != (uint8_t)target_row)
        {
            continue;
        }
        int distance = key_center_twice(&keys[i]) - current_center;
        distance = distance < 0 ? -distance : distance;
        if (distance < nearest_distance)
        {
            candidate = i;
            nearest_distance = distance;
        }
    }
    input->selected_key = (uint8_t)candidate;
}

static bool append_character(UiTextInput *input, char character)
{
    if (input->length >= input->maximum_length)
    {
        return false;
    }
    input->value[input->length++] = character;
    input->value[input->length] = '\0';
    return true;
}

static UiTextInputResult activate_key(UiTextInput *input)
{
    const UiKeyboardKey *key = selected_key(input);
    switch (key->type)
    {
        case UI_KEY_CHARACTER:
        {
            char character = key->character;
            if (input->shifted && character >= 'a' && character <= 'z')
            {
                character = (char)(character - 'a' + 'A');
                if (!input->shift_locked)
                {
                    input->shifted = false;
                }
            }
            return append_character(input, character)
                ? UI_TEXT_INPUT_CHANGED
                : UI_TEXT_INPUT_UNCHANGED;
        }
        case UI_KEY_SHIFT:
            if (!input->shifted)
            {
                input->shifted = true;
                input->shift_locked = false;
            }
            else if (!input->shift_locked)
            {
                input->shift_locked = true;
            }
            else
            {
                input->shifted = false;
                input->shift_locked = false;
            }
            return UI_TEXT_INPUT_CHANGED;
        case UI_KEY_SYMBOLS:
            input->keyboard_page = UI_KEYBOARD_SYMBOLS;
            input->selected_key = 0;
            input->shifted = false;
            input->shift_locked = false;
            return UI_TEXT_INPUT_CHANGED;
        case UI_KEY_LETTERS:
            input->keyboard_page = UI_KEYBOARD_MAIN;
            input->selected_key = UI_KEYBOARD_INITIAL_KEY;
            return UI_TEXT_INPUT_CHANGED;
        case UI_KEY_SPACE:
            return append_character(input, ' ')
                ? UI_TEXT_INPUT_CHANGED
                : UI_TEXT_INPUT_UNCHANGED;
    }
    return UI_TEXT_INPUT_UNCHANGED;
}

UiTextInputResult ui_text_input_handle_event(
    UiTextInput *input,
    const InputEvent *event)
{
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return UI_TEXT_INPUT_UNCHANGED;
    }

    switch (event->button)
    {
        case INPUT_UP:
            move_vertical(input, -1);
            return UI_TEXT_INPUT_CHANGED;
        case INPUT_DOWN:
            move_vertical(input, 1);
            return UI_TEXT_INPUT_CHANGED;
        case INPUT_RIGHT:
            move_horizontal(input, 1);
            return UI_TEXT_INPUT_CHANGED;
        case INPUT_LEFT:
            move_horizontal(input, -1);
            return UI_TEXT_INPUT_CHANGED;
        case INPUT_CLICK:
            return activate_key(input);
        case INPUT_B:
            if (input->length > 0)
            {
                input->value[--input->length] = '\0';
                return UI_TEXT_INPUT_CHANGED;
            }
            return UI_TEXT_INPUT_UNCHANGED;
        case INPUT_A:
            return UI_TEXT_INPUT_SUBMIT;
        default:
            return UI_TEXT_INPUT_UNCHANGED;
    }
}

char ui_text_input_selected_character(const UiTextInput *input)
{
    const UiKeyboardKey *key = selected_key(input);
    if (key->type != UI_KEY_CHARACTER)
    {
        return key->type == UI_KEY_SPACE ? ' ' : '\0';
    }
    if (input->shifted && key->character >= 'a' && key->character <= 'z')
    {
        return (char)(key->character - 'a' + 'A');
    }
    return key->character;
}

size_t ui_list_move(size_t selected, size_t item_count, int direction)
{
    if (item_count == 0)
    {
        return 0;
    }
    if (direction > 0)
    {
        return (selected + 1u) % item_count;
    }
    return selected == 0 ? item_count - 1 : selected - 1;
}

size_t ui_list_first_visible(
    size_t selected,
    size_t current_first,
    size_t visible_count)
{
    if (selected < current_first)
    {
        return selected;
    }
    if (selected >= current_first + visible_count)
    {
        return selected - visible_count + 1;
    }
    return current_first;
}

void ui_draw_header(
    DisplaySurface *surface,
    const char *title)
{
    ui_draw_header_with_detail(surface, title, NULL);
}

void ui_draw_header_with_detail(
    DisplaySurface *surface,
    const char *title,
    const char *detail)
{
    display_draw_text_with_font(
        surface, 8, 3, title, DISPLAY_FONT_SMALL,
        ui_chrome_color(), DISPLAY_COLOR_BLACK);
    if (detail != NULL && detail[0] != '\0')
    {
        const uint16_t detail_width =
            display_text_width(detail, DISPLAY_FONT_SMALL);
        display_draw_text_with_font(
            surface,
            surface->width - detail_width - 8,
            3,
            detail,
            DISPLAY_FONT_SMALL,
            ui_chrome_color(),
            DISPLAY_COLOR_BLACK);
    }
    display_draw_rectangle(
        surface, 5, 17, surface->width - 6, 18,
        ui_accent_color(), 1);
}

void ui_draw_footer(DisplaySurface *surface, const char *legend)
{
    display_draw_text_with_font(
        surface,
        6,
        surface->height - display_font_height(DISPLAY_FONT_TINY) - 2,
        legend,
        DISPLAY_FONT_TINY,
        ui_chrome_color(),
        DISPLAY_COLOR_BLACK);
}

void ui_draw_list_item(
    DisplaySurface *surface,
    uint16_t y,
    const char *label,
    const char *detail,
    bool selected)
{
    ui_draw_menu_item(
        surface, y, label, detail, selected, true);
}

void ui_draw_menu_item(
    DisplaySurface *surface,
    uint16_t y,
    const char *label,
    const char *detail,
    bool selected,
    bool enabled)
{
    const DisplayColor foreground = enabled
        ? ui_menu_text_color()
        : ui_chrome_color();
    uint16_t detail_x = surface->width;
    if (selected)
    {
        for (uint16_t rail_y = y - 3; rail_y <= y + 14; rail_y++)
        {
            display_draw_point(
                surface, 7, rail_y, ui_accent_color(), 2);
        }
    }
    if (detail != NULL && detail[0] != '\0')
    {
        const uint16_t detail_width =
            display_text_width(detail, DISPLAY_FONT_SMALL);
        detail_x = detail_width + 10 < surface->width
            ? surface->width - detail_width - 8
            : 12;
        display_draw_text(
            surface, detail_x, y, detail,
            enabled ? ui_secondary_text_color() : ui_chrome_color(),
            DISPLAY_COLOR_BLACK);
    }

    char rendered_label[UI_TEXT_INPUT_CAPACITY];
    const uint16_t character_width =
        display_text_width("W", DISPLAY_FONT_SMALL);
    const uint16_t available_width = detail_x > 20
        ? detail_x - 20
        : surface->width - 20;
    size_t maximum_characters = character_width == 0
        ? 0
        : available_width / character_width;
    if (maximum_characters >= sizeof(rendered_label))
    {
        maximum_characters = sizeof(rendered_label) - 1;
    }
    snprintf(
        rendered_label,
        sizeof(rendered_label),
        "%.*s",
        (int)maximum_characters,
        label);
    display_draw_text(
        surface, 14, y, rendered_label, foreground, DISPLAY_COLOR_BLACK);
}

void ui_draw_text_input(
    DisplaySurface *surface,
    const UiTextInput *input,
    uint16_t y)
{
    char rendered[UI_TEXT_INPUT_CAPACITY];
    const size_t maximum_visible = 26;
    const size_t visible_length = input->length < maximum_visible
        ? input->length
        : maximum_visible;
    if (input->masked)
    {
        memset(rendered, '*', visible_length);
        rendered[visible_length] = '\0';
    }
    else
    {
        const size_t offset = input->length - visible_length;
        snprintf(rendered, sizeof(rendered), "%s", input->value + offset);
    }

    display_draw_rectangle(
        surface, 7, y, surface->width - 8, y + 20,
        ui_accent_color(), 1);
    display_draw_text(
        surface, 12, y + 4,
        rendered[0] == '\0' ? "<empty>" : rendered,
        ui_menu_text_color(), DISPLAY_COLOR_BLACK);
}

static void keyboard_key_label(
    const UiTextInput *input,
    const UiKeyboardKey *key,
    char label[6])
{
    switch (key->type)
    {
        case UI_KEY_CHARACTER:
            label[0] = key->character;
            if (input->shifted && label[0] >= 'a' && label[0] <= 'z')
            {
                label[0] = (char)(label[0] - 'a' + 'A');
            }
            label[1] = '\0';
            break;
        case UI_KEY_SHIFT:
            snprintf(label, 6, input->shift_locked ? "CAP" : "SH");
            break;
        case UI_KEY_SYMBOLS:
            snprintf(label, 6, "#+=");
            break;
        case UI_KEY_LETTERS:
            snprintf(label, 6, "abc");
            break;
        case UI_KEY_SPACE:
            snprintf(label, 6, "SPACE");
            break;
    }
}

void ui_draw_virtual_keyboard(
    DisplaySurface *surface,
    const UiTextInput *input,
    uint16_t y)
{
    size_t count;
    const UiKeyboardKey *keys = keyboard_keys(input, &count);
    for (size_t i = 0; i < count; i++)
    {
        const UiKeyboardKey *key = &keys[i];
        const uint16_t x0 = (uint16_t)(4 + key->column * 23);
        const uint16_t x1 = (uint16_t)(x0 + key->span * 23 - 3);
        const uint16_t y0 = (uint16_t)(y + key->row * 16);
        const uint16_t y1 = (uint16_t)(y0 + 14);
        const bool is_selected = i == input->selected_key;
        const bool is_active_shift =
            key->type == UI_KEY_SHIFT && input->shifted;
        const DisplayColor color = is_selected || is_active_shift
            ? ui_accent_color()
            : ui_chrome_color();

        display_draw_rectangle(
            surface, x0, y0, x1, y1,
            color, is_selected ? 2 : 1);

        char label[6];
        keyboard_key_label(input, key, label);
        const uint16_t label_width =
            display_text_width(label, DISPLAY_FONT_SMALL);
        const uint16_t key_width = x1 - x0 + 1;
        const uint16_t label_x = label_width < key_width
            ? (uint16_t)(x0 + (key_width - label_width) / 2)
            : x0;
        display_draw_text(
            surface, label_x, (uint16_t)(y0 + 1), label,
            is_selected || is_active_shift
                ? ui_menu_text_color()
                : ui_secondary_text_color(),
            DISPLAY_COLOR_BLACK);
    }
}
