#include "launcher_app.h"

#include "ui.h"

typedef struct LauncherState
{
    AppContext *context;
    size_t selected_index;
    bool help_visible;
} LauncherState;

static bool application_is_visible(const LauncherState *state, size_t index)
{
    return app_is_available(
        state->context->applications[index],
        state->context->available_capabilities);
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
            return;
        }
    }
}

static bool launcher_start(void *state_memory, AppContext *context)
{
    LauncherState *state = state_memory;
    state->context = context;
    state->selected_index = 0;
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
    else if (event->button == INPUT_B &&
             state->context->application_count > 0 &&
             application_is_visible(state, state->selected_index))
    {
        state->context->request_restart(
            state->context->manager,
            state->selected_index);
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
        surface, 8, 60, "B           Restart",
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
    bool has_visible_application = false;
    for (size_t i = 0; i < state->context->application_count; i++)
    {
        if (!application_is_visible(state, i))
        {
            continue;
        }

        has_visible_application = true;
        const bool selected = i == state->selected_index;
        ui_draw_menu_item(
            surface, y,
            state->context->applications[i]->name,
            "",
            selected,
            true);
        y += 24;
    }

    if (!has_visible_application)
    {
        display_draw_text(
            surface, 18, 30, "No available apps",
            ui_error_color(), DISPLAY_COLOR_BLACK);
    }

    ui_draw_footer(surface, ">/CLICK:OPEN A:HELP B:RESTART");
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
