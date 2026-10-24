#include "ssh_app.h"

#include <stdio.h>
#include <string.h>

#include "platform/network.h"
#include "platform/ssh.h"
#include "ssh_defaults.h"
#include "ui.h"

#define SSH_APP_PORT 22
#define SSH_REFRESH_INTERVAL_MS 100
#define SSH_SETUP_ITEM_COUNT 4

typedef enum SshAppScreen
{
    SSH_APP_SETUP,
    SSH_APP_EDIT_HOST,
    SSH_APP_EDIT_USER,
    SSH_APP_EDIT_PASSWORD,
    SSH_APP_STATUS,
    SSH_APP_TERMINAL,
    SSH_APP_COMMAND,
} SshAppScreen;

typedef struct SshAppState
{
    AppContext *context;
    SshSnapshot snapshot;
    SshAppScreen screen;
    UiTextInput host;
    UiTextInput username;
    UiTextInput password;
    UiTextInput command;
    size_t selected_item;
    uint32_t refresh_elapsed_ms;
    char local_message[SSH_MESSAGE_CAPACITY];
} SshAppState;

static void refresh_snapshot(SshAppState *state)
{
    state->context->ssh->get_snapshot(&state->snapshot);
}

static bool ssh_app_start(void *state_memory, AppContext *context)
{
    if (context->ssh == NULL || context->network == NULL)
    {
        return false;
    }

    SshAppState *state = state_memory;
    *state = (SshAppState){.context = context};
    ui_text_input_init(&state->host, SSH_HOST_CAPACITY - 1, false);
    ui_text_input_init(&state->username, SSH_USERNAME_CAPACITY - 1, false);
    ui_text_input_init(&state->password, SSH_PASSWORD_CAPACITY - 1, true);
    ui_text_input_init(&state->command, SSH_LINE_CAPACITY - 1, false);
    snprintf(
        state->host.value,
        sizeof(state->host.value),
        "%s",
        PICO_LCD_DEFAULT_SSH_HOST);
    state->host.length = strlen(state->host.value);
    snprintf(
        state->username.value,
        sizeof(state->username.value),
        "%s",
        PICO_LCD_DEFAULT_SSH_USERNAME);
    state->username.length = strlen(state->username.value);
    snprintf(
        state->password.value,
        sizeof(state->password.value),
        "%s",
        PICO_LCD_DEFAULT_SSH_PASSWORD);
    state->password.length = strlen(state->password.value);
    refresh_snapshot(state);
    if (state->snapshot.status == SSH_STATUS_CONNECTED)
    {
        state->screen = SSH_APP_TERMINAL;
    }
    return true;
}

static void ssh_app_stop(void *state_memory)
{
    SshAppState *state = state_memory;
    ui_text_input_clear(&state->password);
    ui_text_input_clear(&state->command);
    refresh_snapshot(state);
    if (state->snapshot.status == SSH_STATUS_CONNECTING ||
        state->snapshot.status == SSH_STATUS_VERIFY_HOST ||
        state->snapshot.status == SSH_STATUS_AUTHENTICATING ||
        state->snapshot.status == SSH_STATUS_CONNECTED)
    {
        state->context->ssh->request_disconnect();
    }
}

static UiTextInput *active_editor(SshAppState *state)
{
    switch (state->screen)
    {
        case SSH_APP_EDIT_HOST:
            return &state->host;
        case SSH_APP_EDIT_USER:
            return &state->username;
        case SSH_APP_EDIT_PASSWORD:
            return &state->password;
        case SSH_APP_COMMAND:
            return &state->command;
        default:
            return NULL;
    }
}

static bool is_editor_screen(SshAppScreen screen)
{
    return screen == SSH_APP_EDIT_HOST ||
        screen == SSH_APP_EDIT_USER ||
        screen == SSH_APP_EDIT_PASSWORD ||
        screen == SSH_APP_COMMAND;
}

static void begin_connection(SshAppState *state)
{
    state->local_message[0] = '\0';
    if (state->host.length == 0 || state->username.length == 0)
    {
        snprintf(
            state->local_message,
            sizeof(state->local_message),
            "Host and user required");
        return;
    }

    NetworkSnapshot network;
    state->context->network->get_snapshot(&network);
    if (network.status != NETWORK_STATUS_CONNECTED)
    {
        snprintf(
            state->local_message,
            sizeof(state->local_message),
            "Wi-Fi offline");
        return;
    }

    if (!state->context->ssh->request_connect(
            state->host.value,
            SSH_APP_PORT,
            state->username.value,
            state->password.value))
    {
        snprintf(
            state->local_message,
            sizeof(state->local_message),
            "SSH service busy");
        return;
    }

    state->screen = SSH_APP_STATUS;
}

static AppResult handle_setup(
    SshAppState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_UP || event->button == INPUT_DOWN)
    {
        state->selected_item = ui_list_move(
            state->selected_item,
            SSH_SETUP_ITEM_COUNT,
            event->button == INPUT_UP ? -1 : 1);
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        return APP_RESULT_HOME;
    }
    if (event->button != INPUT_RIGHT && event->button != INPUT_CLICK)
    {
        return APP_RESULT_CONTINUE;
    }

    state->local_message[0] = '\0';
    if (state->selected_item == 0)
    {
        state->screen = SSH_APP_EDIT_HOST;
    }
    else if (state->selected_item == 1)
    {
        state->screen = SSH_APP_EDIT_USER;
    }
    else if (state->selected_item == 2)
    {
        state->screen = SSH_APP_EDIT_PASSWORD;
    }
    else
    {
        begin_connection(state);
    }
    return APP_RESULT_REDRAW;
}

static AppResult handle_editor(
    SshAppState *state,
    const InputEvent *event)
{
    UiTextInput *editor = active_editor(state);
    const UiTextInputResult result =
        ui_text_input_handle_event(editor, event);
    if (result == UI_TEXT_INPUT_SUBMIT)
    {
        if (state->screen == SSH_APP_COMMAND)
        {
            if (state->context->ssh->request_send_line(editor->value))
            {
                ui_text_input_clear(editor);
                state->screen = SSH_APP_TERMINAL;
            }
        }
        else
        {
            state->screen = SSH_APP_SETUP;
        }
        return APP_RESULT_REDRAW;
    }
    return result == UI_TEXT_INPUT_CHANGED
        ? APP_RESULT_REDRAW
        : APP_RESULT_CONTINUE;
}

static AppResult handle_status(
    SshAppState *state,
    const InputEvent *event)
{
    if (state->snapshot.status == SSH_STATUS_VERIFY_HOST)
    {
        if (event->button == INPUT_A)
        {
            state->context->ssh->request_host_verification(true);
            return APP_RESULT_REDRAW;
        }
        if (event->button == INPUT_B || event->button == INPUT_LEFT)
        {
            state->context->ssh->request_host_verification(false);
            state->screen = SSH_APP_SETUP;
            return APP_RESULT_REDRAW;
        }
    }
    else if (event->button == INPUT_B || event->button == INPUT_LEFT)
    {
        state->context->ssh->request_disconnect();
        state->screen = SSH_APP_SETUP;
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_terminal(
    SshAppState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_CLICK || event->button == INPUT_RIGHT)
    {
        state->screen = SSH_APP_COMMAND;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_B || event->button == INPUT_LEFT)
    {
        return APP_RESULT_HOME;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult ssh_app_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    SshAppState *state = state_memory;
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    if (active_editor(state) != NULL)
    {
        return handle_editor(state, event);
    }
    if (state->screen == SSH_APP_SETUP)
    {
        return handle_setup(state, event);
    }
    if (state->screen == SSH_APP_TERMINAL)
    {
        return handle_terminal(state, event);
    }
    return handle_status(state, event);
}

static AppResult ssh_app_update(void *state_memory, uint32_t elapsed_ms)
{
    SshAppState *state = state_memory;
    state->refresh_elapsed_ms += elapsed_ms;
    if (state->refresh_elapsed_ms < SSH_REFRESH_INTERVAL_MS)
    {
        return APP_RESULT_CONTINUE;
    }
    state->refresh_elapsed_ms = 0;

    const uint32_t previous_revision = state->snapshot.revision;
    refresh_snapshot(state);
    if (state->snapshot.status == SSH_STATUS_CONNECTED &&
        state->screen == SSH_APP_STATUS)
    {
        state->screen = SSH_APP_TERMINAL;
    }
    if (state->snapshot.status != SSH_STATUS_CONNECTED &&
        state->screen == SSH_APP_TERMINAL)
    {
        state->screen = SSH_APP_STATUS;
    }
    return previous_revision != state->snapshot.revision
        ? APP_RESULT_REDRAW
        : APP_RESULT_CONTINUE;
}

static const char *masked_password(const UiTextInput *password)
{
    return password->length == 0 ? "<empty>" : "********";
}

static const char *visible_tail(const UiTextInput *input, size_t characters)
{
    return input->length > characters
        ? input->value + input->length - characters
        : input->value;
}

static void render_setup(
    const SshAppState *state,
    DisplaySurface *surface)
{
    const uint16_t spacing = surface->height >= 200 ? 28 : 20;
    const uint16_t first_y = 28;
    ui_draw_menu_item(
        surface, first_y, "Host",
        state->host.length == 0 ? "<empty>" : visible_tail(&state->host, 18),
        state->selected_item == 0, true);
    ui_draw_menu_item(
        surface, first_y + spacing, "User",
        state->username.length == 0
            ? "<empty>"
            : visible_tail(&state->username, 14),
        state->selected_item == 1, true);
    ui_draw_menu_item(
        surface, first_y + spacing * 2, "Password",
        masked_password(&state->password),
        state->selected_item == 2, true);
    ui_draw_menu_item(
        surface, first_y + spacing * 3, "Connect", "port 22",
        state->selected_item == 3, true);
    if (state->local_message[0] != '\0')
    {
        display_draw_text_with_font(
            surface, 10,
            surface->height >= 200 ? 150 : 109,
            state->local_message,
            surface->height >= 200 ? DISPLAY_FONT_SMALL : DISPLAY_FONT_TINY,
            ui_error_color(), DISPLAY_COLOR_BLACK);
    }
    ui_draw_footer(surface, ">/CLICK:EDIT B:HOME");
}

static void render_editor(
    const SshAppState *state,
    DisplaySurface *surface)
{
    const UiTextInput *editor = NULL;
    const char *title = "SSH";
    if (state->screen == SSH_APP_EDIT_HOST)
    {
        editor = &state->host;
        title = "SSH HOST";
    }
    else if (state->screen == SSH_APP_EDIT_USER)
    {
        editor = &state->username;
        title = "SSH USER";
    }
    else if (state->screen == SSH_APP_EDIT_PASSWORD)
    {
        editor = &state->password;
        title = "SSH PASSWORD";
    }
    else
    {
        editor = &state->command;
        title = "SSH COMMAND";
    }

    if (surface->height >= 200)
    {
        ui_draw_header(surface, title);
        ui_draw_text_input(surface, editor, 23);
        ui_draw_virtual_keyboard(surface, editor, 49);
    }
    else
    {
        display_draw_text_with_font(
            surface, 5, 1, title, DISPLAY_FONT_SMALL,
            ui_chrome_color(), DISPLAY_COLOR_BLACK);
        ui_draw_text_input(surface, editor, 14);
        ui_draw_virtual_keyboard(surface, editor, 38);
    }
    ui_draw_footer(surface, "JOY:MOVE CLICK:ADD B:DEL A:ENTER");
}

static void draw_wrapped(
    DisplaySurface *surface,
    uint16_t x,
    uint16_t y,
    const char *text,
    size_t columns,
    DisplayColor color)
{
    char line[33];
    while (*text != '\0' && y + 10 < surface->height)
    {
        size_t count = strlen(text);
        if (count > columns)
        {
            count = columns;
        }
        memcpy(line, text, count);
        line[count] = '\0';
        display_draw_text_with_font(
            surface, x, y, line, DISPLAY_FONT_TINY,
            color, DISPLAY_COLOR_BLACK);
        text += count;
        y = (uint16_t)(y + display_font_height(DISPLAY_FONT_TINY) + 1);
    }
}

static void render_status(
    const SshAppState *state,
    DisplaySurface *surface)
{
    display_draw_text_with_font(
        surface, 10, 28, ssh_status_name(state->snapshot.status),
        DISPLAY_FONT_MEDIUM,
        state->snapshot.status == SSH_STATUS_ERROR ||
            state->snapshot.status == SSH_STATUS_BAD_CREDENTIALS
            ? ui_error_color()
            : ui_activity_color(),
        DISPLAY_COLOR_BLACK);
    display_draw_text_with_font(
        surface, 10, 52, state->snapshot.message,
        DISPLAY_FONT_SMALL, ui_secondary_text_color(), DISPLAY_COLOR_BLACK);
    if (state->snapshot.status == SSH_STATUS_VERIFY_HOST)
    {
        display_draw_text_with_font(
            surface, 10, 72, "HOST FINGERPRINT", DISPLAY_FONT_SMALL,
            ui_primary_text_color(), DISPLAY_COLOR_BLACK);
        draw_wrapped(
            surface, 10, 88, state->snapshot.fingerprint,
            surface->height >= 200 ? 30 : 24,
            ui_neutral_text_color());
        ui_draw_footer(surface, "A:TRUST B:REJECT");
    }
    else
    {
        ui_draw_footer(surface, "B:BACK");
    }
}

static void render_terminal(
    const SshAppState *state,
    DisplaySurface *surface)
{
    const DisplayFont font = DISPLAY_FONT_TINY;
    const uint16_t line_height = display_font_height(font) + 1;
    const size_t columns = surface->width /
        display_text_width("W", font);
    const size_t rows = (surface->height - 34) / line_height;
    const char *terminal = state->snapshot.terminal;
    const char *line_starts[24];
    size_t line_count = 0;
    line_starts[line_count++] = terminal;
    for (const char *cursor = terminal; *cursor != '\0'; cursor++)
    {
        if (*cursor == '\n')
        {
            if (line_count == 24)
            {
                memmove(
                    line_starts,
                    line_starts + 1,
                    (line_count - 1) * sizeof(line_starts[0]));
                line_count--;
            }
            line_starts[line_count++] = cursor + 1;
        }
    }
    const size_t first = line_count > rows ? line_count - rows : 0;
    uint16_t y = 22;
    for (size_t i = first; i < line_count && i - first < rows; i++)
    {
        char line[48];
        size_t count = strcspn(line_starts[i], "\n");
        if (count > columns)
        {
            count = columns;
        }
        if (count >= sizeof(line))
        {
            count = sizeof(line) - 1;
        }
        memcpy(line, line_starts[i], count);
        line[count] = '\0';
        display_draw_text_with_font(
            surface, 3, y, line, font,
            ui_primary_text_color(), DISPLAY_COLOR_BLACK);
        y = (uint16_t)(y + line_height);
    }
    ui_draw_footer(surface, ">/CLICK:TYPE B:HOME");
}

static void ssh_app_render(
    const void *state_memory,
    DisplaySurface *surface)
{
    const SshAppState *state = state_memory;
    display_clear(surface, DISPLAY_COLOR_BLACK);
    if (is_editor_screen(state->screen))
    {
        render_editor(state, surface);
        return;
    }

    ui_draw_header(surface, "SSH");
    if (state->screen == SSH_APP_SETUP)
    {
        render_setup(state, surface);
    }
    else if (state->screen == SSH_APP_TERMINAL)
    {
        render_terminal(state, surface);
    }
    else
    {
        render_status(state, surface);
    }
}

const Application ssh_application = {
    .name = "SSH",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT | APP_CAP_WIFI,
    .state_size = sizeof(SshAppState),
    .state_policy = APP_STATE_TRANSIENT,
    .start = ssh_app_start,
    .pause = NULL,
    .resume = NULL,
    .stop = ssh_app_stop,
    .handle_event = ssh_app_handle_event,
    .update = ssh_app_update,
    .render = ssh_app_render,
};
