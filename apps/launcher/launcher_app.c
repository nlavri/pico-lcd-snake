#include "launcher_app.h"

#include "ui.h"

typedef struct LauncherState
{
    AppContext *context;
    size_t selected_index;
    size_t first_visible_row;
    bool help_visible;
} LauncherState;

static bool application_is_visible(const LauncherState *state, size_t index)
{
    return app_is_available(
        state->context->applications[index],
        state->context->available_capabilities);
}

static size_t visible_row_count(const LauncherState *state)
{
    if (state->context->display_height >= 300)
    {
        return 9;
    }
    return state->context->display_height >= 200 ? 6 : 5;
}

static size_t selected_visible_row(const LauncherState *state)
{
    size_t row = 0;
    for (size_t i = 0; i < state->selected_index; i++)
    {
        if (application_is_visible(state, i))
        {
            row++;
        }
    }
    return row;
}

static void keep_selection_visible(LauncherState *state)
{
    state->first_visible_row = ui_list_first_visible(
        selected_visible_row(state),
        state->first_visible_row,
        visible_row_count(state));
}

static bool select_first_application(LauncherState *state)
{
    for (size_t i = 0; i < state->context->application_count; i++)
    {
        if (application_is_visible(state, i))
        {
            state->selected_index = i;
            return true;
        }
    }
    return false;
}

static void move_selection(LauncherState *state, int direction)
{
    if (state->context->application_count == 0)
    {
        return;
    }

    size_t candidate = state->selected_index;
    for (size_t i = 0; i < state->context->application_count; i++)
    {
        if (direction > 0)
        {
            candidate = (candidate + 1) % state->context->application_count;
        }
        else
        {
            candidate = candidate == 0
                ? state->context->application_count - 1
                : candidate - 1;
        }

        if (application_is_visible(state, candidate))
        {
            state->selected_index = candidate;
            keep_selection_visible(state);
            return;
        }
    }
}

static bool application_at_visible_row(
    const LauncherState *state,
    size_t requested_row,
    size_t *application_index)
{
    size_t visible_row = 0;
    for (size_t i = 0; i < state->context->application_count; i++)
    {
        if (!application_is_visible(state, i))
        {
            continue;
        }
        if (visible_row == requested_row)
        {
            *application_index = i;
            return true;
        }
        visible_row++;
    }
    return false;
}

static bool launcher_start(void *state_memory, AppContext *context)
{
    LauncherState *state = state_memory;
    state->context = context;
    state->selected_index = 0;
    state->first_visible_row = 0;
    state->help_visible = false;
    select_first_application(state);
    return true;
}

static void launcher_resume(void *state_memory)
{
    LauncherState *state = state_memory;
    state->help_visible = false;
}

static AppResult launcher_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    LauncherState *state = state_memory;
    if (event->button == INPUT_TOUCH && event->action == INPUT_PRESSED)
    {
        if (state->help_visible)
        {
            state->help_visible = false;
            return APP_RESULT_REDRAW;
        }

        const uint16_t row_top = 22;
        const uint16_t row_spacing =
            state->context->display_height >= 200 ? 28 : 20;
        const uint16_t list_bottom = (uint16_t)(
            row_top + visible_row_count(state) * row_spacing);
        if (event->y >= row_top && event->y < list_bottom)
        {
            const size_t visible_row = state->first_visible_row +
                (event->y - row_top) / row_spacing;
            size_t application_index;
            if (application_at_visible_row(
                    state, visible_row, &application_index))
            {
                state->selected_index = application_index;
                state->context->request_launch(
                    state->context->manager,
                    state->selected_index);
            }
        }
        return APP_RESULT_CONTINUE;
    }
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    if ((event->button == INPUT_CLICK || event->button == INPUT_LEFT) &&
        state->help_visible)
    {
        state->help_visible = false;
        return APP_RESULT_REDRAW;
    }

    if (state->help_visible)
    {
        return APP_RESULT_CONTINUE;
    }

    if (event->button == INPUT_UP)
    {
        move_selection(state, -1);
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_DOWN)
    {
        move_selection(state, 1);
        return APP_RESULT_REDRAW;
    }
    if ((event->button == INPUT_RIGHT || event->button == INPUT_CLICK) &&
        state->context->application_count > 0 &&
        application_is_visible(state, state->selected_index))
    {
        state->context->request_launch(
            state->context->manager,
            state->selected_index);
    }
    else if (event->button == INPUT_A)
    {
        state->help_visible = true;
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static void render_help(DisplaySurface *surface)
{
    display_clear(surface, DISPLAY_COLOR_BLACK);
    ui_draw_header(surface, "LAUNCHER HELP");
    display_draw_text(
        surface, 8, 24, "UP/DOWN  Select app",
        ui_menu_text_color(), DISPLAY_COLOR_BLACK);
    display_draw_text(
        surface, 8, 42, "RIGHT/CLICK Open app",
        ui_menu_text_color(), DISPLAY_COLOR_BLACK);
    display_draw_text(
        surface, 8, 60, "B           No action at home",
        ui_menu_text_color(), DISPLAY_COLOR_BLACK);
    display_draw_text(
        surface, 8, 78, "A           Help",
        ui_menu_text_color(), DISPLAY_COLOR_BLACK);
    display_draw_text(
        surface, 8, 96, "A+B      Return home (from app)",
        ui_menu_text_color(), DISPLAY_COLOR_BLACK);
    ui_draw_footer(surface, "LEFT/CLICK:BACK");
}

static void launcher_render(const void *state_memory, DisplaySurface *surface)
{
    const LauncherState *state = state_memory;
    if (state->help_visible)
    {
        render_help(surface);
        return;
    }

    display_clear(surface, DISPLAY_COLOR_BLACK);
    ui_draw_header(surface, "APPLICATIONS");

    uint16_t y = 26;
    const uint16_t row_spacing = surface->height >= 200 ? 28 : 20;
    bool has_visible_application = false;
    size_t visible_row = 0;
    const size_t maximum_rows = visible_row_count(state);
    for (size_t i = 0; i < state->context->application_count; i++)
    {
        if (!application_is_visible(state, i))
        {
            continue;
        }

        has_visible_application = true;
        if (visible_row++ < state->first_visible_row)
        {
            continue;
        }
        if (visible_row > state->first_visible_row + maximum_rows)
        {
            break;
        }
        const bool selected = i == state->selected_index;
        ui_draw_menu_item(
            surface, y,
            state->context->applications[i]->name,
            "",
            selected,
            true);
        y += row_spacing;
    }

    if (!has_visible_application)
    {
        display_draw_text(
            surface, 18, 30, "No available apps",
            ui_error_color(), DISPLAY_COLOR_BLACK);
    }

    ui_draw_footer(surface, ">/CLICK/TAP:OPEN A:HELP");
}

const Application launcher_application = {
    .name = "Launcher",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(LauncherState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = launcher_start,
    .pause = NULL,
    .resume = launcher_resume,
    .stop = NULL,
    .handle_event = launcher_handle_event,
    .update = NULL,
    .render = launcher_render,
};
