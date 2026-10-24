#include "calculator_app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "calculator_engine.h"
#include "ui.h"

#define CALCULATOR_EXPRESSION_CAPACITY 65
#define CALCULATOR_RESULT_CAPACITY 32
#define CALCULATOR_COLUMNS 5
#define CALCULATOR_ROWS 4

typedef enum CalculatorKeyAction
{
    CALCULATOR_KEY_INSERT,
    CALCULATOR_KEY_CLEAR,
    CALCULATOR_KEY_DELETE,
    CALCULATOR_KEY_EVALUATE,
} CalculatorKeyAction;

typedef struct CalculatorKey
{
    const char *label;
    char value;
    CalculatorKeyAction action;
} CalculatorKey;

typedef struct CalculatorAppState
{
    char expression[CALCULATOR_EXPRESSION_CAPACITY];
    char result[CALCULATOR_RESULT_CAPACITY];
    const char *error;
    size_t length;
    uint8_t selected_row;
    uint8_t selected_column;
    bool evaluated;
} CalculatorAppState;

static const CalculatorKey calculator_keys[CALCULATOR_ROWS][CALCULATOR_COLUMNS] = {
    {{"(", '(', CALCULATOR_KEY_INSERT}, {")", ')', CALCULATOR_KEY_INSERT},
     {"C", 0, CALCULATOR_KEY_CLEAR}, {"DEL", 0, CALCULATOR_KEY_DELETE},
     {"/", '/', CALCULATOR_KEY_INSERT}},
    {{"7", '7', CALCULATOR_KEY_INSERT}, {"8", '8', CALCULATOR_KEY_INSERT},
     {"9", '9', CALCULATOR_KEY_INSERT}, {"*", '*', CALCULATOR_KEY_INSERT},
     {"-", '-', CALCULATOR_KEY_INSERT}},
    {{"4", '4', CALCULATOR_KEY_INSERT}, {"5", '5', CALCULATOR_KEY_INSERT},
     {"6", '6', CALCULATOR_KEY_INSERT}, {"+", '+', CALCULATOR_KEY_INSERT},
     {".", '.', CALCULATOR_KEY_INSERT}},
    {{"1", '1', CALCULATOR_KEY_INSERT}, {"2", '2', CALCULATOR_KEY_INSERT},
     {"3", '3', CALCULATOR_KEY_INSERT}, {"0", '0', CALCULATOR_KEY_INSERT},
     {"=", 0, CALCULATOR_KEY_EVALUATE}},
};

static bool calculator_start(void *state_memory, AppContext *context)
{
    (void)context;
    CalculatorAppState *state = state_memory;
    *state = (CalculatorAppState){0};
    return true;
}

static bool is_operator(char character)
{
    return character == '+' || character == '-' ||
        character == '*' || character == '/';
}

static void clear_feedback(CalculatorAppState *state)
{
    state->result[0] = '\0';
    state->error = NULL;
}

static void clear_expression(CalculatorAppState *state)
{
    state->expression[0] = '\0';
    state->length = 0;
    state->evaluated = false;
    clear_feedback(state);
}

static void delete_character(CalculatorAppState *state)
{
    if (state->length > 0)
    {
        state->length--;
        state->expression[state->length] = '\0';
    }
    state->evaluated = false;
    clear_feedback(state);
}

static void insert_character(CalculatorAppState *state, char character)
{
    if (state->evaluated)
    {
        if (is_operator(character) && state->result[0] != '\0')
        {
            snprintf(
                state->expression,
                sizeof(state->expression),
                "%s",
                state->result);
            state->length = strlen(state->expression);
        }
        else
        {
            state->length = 0;
            state->expression[0] = '\0';
        }
    }

    state->evaluated = false;
    clear_feedback(state);
    if (state->length + 1 >= sizeof(state->expression))
    {
        state->error = "Expression full";
        return;
    }
    state->expression[state->length++] = character;
    state->expression[state->length] = '\0';
}

static void evaluate_expression(CalculatorAppState *state)
{
    double value = 0.0;
    clear_feedback(state);
    switch (calculator_evaluate(state->expression, &value))
    {
        case CALCULATOR_RESULT_OK:
            snprintf(state->result, sizeof(state->result), "%.10g", value);
            state->evaluated = true;
            break;
        case CALCULATOR_RESULT_DIVIDE_BY_ZERO:
            state->error = "Cannot divide by zero";
            break;
        case CALCULATOR_RESULT_SYNTAX_ERROR:
        default:
            state->error = "Syntax error";
            break;
    }
}

static void activate_selected_key(CalculatorAppState *state)
{
    const CalculatorKey *key =
        &calculator_keys[state->selected_row][state->selected_column];
    switch (key->action)
    {
        case CALCULATOR_KEY_INSERT:
            insert_character(state, key->value);
            break;
        case CALCULATOR_KEY_CLEAR:
            clear_expression(state);
            break;
        case CALCULATOR_KEY_DELETE:
            delete_character(state);
            break;
        case CALCULATOR_KEY_EVALUATE:
            evaluate_expression(state);
            break;
    }
}

static AppResult calculator_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    CalculatorAppState *state = state_memory;
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    switch (event->button)
    {
        case INPUT_UP:
            state->selected_row = state->selected_row == 0
                ? CALCULATOR_ROWS - 1
                : state->selected_row - 1;
            return APP_RESULT_REDRAW;
        case INPUT_DOWN:
            state->selected_row =
                (state->selected_row + 1) % CALCULATOR_ROWS;
            return APP_RESULT_REDRAW;
        case INPUT_LEFT:
            state->selected_column = state->selected_column == 0
                ? CALCULATOR_COLUMNS - 1
                : state->selected_column - 1;
            return APP_RESULT_REDRAW;
        case INPUT_RIGHT:
            state->selected_column =
                (state->selected_column + 1) % CALCULATOR_COLUMNS;
            return APP_RESULT_REDRAW;
        case INPUT_CLICK:
            if (event->action == INPUT_PRESSED)
            {
                activate_selected_key(state);
                return APP_RESULT_REDRAW;
            }
            break;
        case INPUT_A:
            if (event->action == INPUT_PRESSED)
            {
                evaluate_expression(state);
                return APP_RESULT_REDRAW;
            }
            break;
        case INPUT_B:
            if (event->action == INPUT_PRESSED)
            {
                if (state->length == 0)
                {
                    return APP_RESULT_HOME;
                }
                delete_character(state);
                return APP_RESULT_REDRAW;
            }
            break;
        default:
            break;
    }
    return APP_RESULT_CONTINUE;
}

static const char *visible_expression(
    const CalculatorAppState *state,
    size_t maximum_characters)
{
    if (state->length <= maximum_characters)
    {
        return state->expression;
    }
    return state->expression + state->length - maximum_characters;
}

static void draw_key(
    DisplaySurface *surface,
    const CalculatorKey *key,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    DisplayFont font,
    bool selected)
{
    const DisplayColor border = selected
        ? ui_accent_color()
        : ui_chrome_color();
    const DisplayColor text = selected
        ? ui_neutral_text_color()
        : ui_menu_text_color();
    display_draw_rectangle(
        surface, x, y, x + width - 1, y + height - 1, border,
        selected ? 2 : 1);
    const uint16_t text_width = display_text_width(key->label, font);
    const uint16_t text_height = display_font_height(font);
    display_draw_text_with_font(
        surface,
        x + (width - text_width) / 2,
        y + (height - text_height) / 2,
        key->label,
        font,
        text,
        DISPLAY_COLOR_BLACK);
}

static void calculator_render(
    const void *state_memory,
    DisplaySurface *surface)
{
    const CalculatorAppState *state = state_memory;
    const bool expanded = surface->height >= 200;
    const DisplayFont expression_font = expanded
        ? DISPLAY_FONT_COMPACT
        : DISPLAY_FONT_SMALL;
    const size_t expression_columns =
        (surface->width - 16) / display_text_width("0", expression_font);

    display_clear(surface, DISPLAY_COLOR_BLACK);
    ui_draw_header(surface, "CALCULATOR");
    display_draw_text_with_font(
        surface,
        8,
        expanded ? 27 : 21,
        state->length == 0
            ? "0"
            : visible_expression(state, expression_columns),
        expression_font,
        ui_secondary_text_color(),
        DISPLAY_COLOR_BLACK);

    const char *answer = state->error != NULL
        ? state->error
        : (state->result[0] != '\0' ? state->result : "");
    display_draw_text_with_font(
        surface,
        8,
        expanded ? 53 : 38,
        answer,
        state->error != NULL ? DISPLAY_FONT_SMALL : DISPLAY_FONT_LARGE,
        state->error != NULL ? ui_error_color() : ui_neutral_text_color(),
        DISPLAY_COLOR_BLACK);

    const uint16_t keypad_y = expanded ? 91 : 65;
    const uint16_t row_step = expanded ? 30 : 14;
    const uint16_t key_height = expanded ? 27 : 13;
    const uint16_t margin = 4;
    const uint16_t gap = 2;
    const uint16_t key_width =
        (surface->width - margin * 2 - gap * (CALCULATOR_COLUMNS - 1)) /
        CALCULATOR_COLUMNS;
    const DisplayFont key_font = expanded
        ? DISPLAY_FONT_COMPACT
        : DISPLAY_FONT_SMALL;

    for (uint8_t row = 0; row < CALCULATOR_ROWS; row++)
    {
        for (uint8_t column = 0; column < CALCULATOR_COLUMNS; column++)
        {
            draw_key(
                surface,
                &calculator_keys[row][column],
                margin + column * (key_width + gap),
                keypad_y + row * row_step,
                key_width,
                key_height,
                key_font,
                row == state->selected_row &&
                    column == state->selected_column);
        }
    }

    ui_draw_footer(surface, "JOY:MOVE CLK:KEY A:= B:DEL");
}

const Application calculator_application = {
    .name = "Calculator",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(CalculatorAppState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = calculator_start,
    .pause = NULL,
    .resume = NULL,
    .stop = NULL,
    .handle_event = calculator_handle_event,
    .update = NULL,
    .render = calculator_render,
};
