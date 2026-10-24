#include "settings_app.h"

#include <stdio.h>
#include <string.h>

#include "platform/network.h"
#include "ui.h"

#define SETTINGS_REFRESH_INTERVAL_MS 200
#define SETTINGS_VISIBLE_NETWORKS 4
#define SETTINGS_VISIBLE_ACCENTS 5
#define SETTINGS_TALL_DISPLAY_HEIGHT 200

typedef enum SettingsMenuItem
{
    SETTINGS_MENU_WIFI,
    SETTINGS_MENU_ACCENT,
    SETTINGS_MENU_BRIGHTNESS,
    SETTINGS_MENU_ITEM_COUNT,
} SettingsMenuItem;

typedef enum SettingsScreen
{
    SETTINGS_SCREEN_MENU,
    SETTINGS_SCREEN_STATUS,
    SETTINGS_SCREEN_NETWORKS,
    SETTINGS_SCREEN_PASSWORD,
    SETTINGS_SCREEN_ACCENTS,
    SETTINGS_SCREEN_BRIGHTNESS,
} SettingsScreen;

typedef struct SettingsState
{
    AppContext *context;
    NetworkSnapshot network;
    SettingsScreen screen;
    size_t selected_menu;
    size_t selected_network;
    size_t first_visible_network;
    size_t selected_accent_position;
    size_t first_visible_accent;
    UiAccent original_accent;
    uint8_t original_brightness;
    NetworkAccessPoint pending_network;
    UiTextInput password;
    uint32_t refresh_elapsed_ms;
    char requested_ssid[NETWORK_SSID_CAPACITY];
    bool connection_requested;
    bool password_too_short;
} SettingsState;

static void refresh_snapshot(SettingsState *state)
{
    state->context->network->get_snapshot(&state->network);
}

static bool settings_start(void *state_memory, AppContext *context)
{
    if (context->network == NULL)
    {
        return false;
    }

    SettingsState *state = state_memory;
    state->context = context;
    state->screen = SETTINGS_SCREEN_MENU;
    state->selected_menu = SETTINGS_MENU_WIFI;
    state->selected_accent_position = 0;
    state->original_accent = ui_accent();
    state->original_brightness = ui_brightness();
    ui_text_input_init(
        &state->password,
        NETWORK_PASSWORD_CAPACITY - 1,
        true);
    refresh_snapshot(state);
    return true;
}

static void settings_stop(void *state_memory)
{
    SettingsState *state = state_memory;
    if (state->screen == SETTINGS_SCREEN_ACCENTS)
    {
        ui_preview_accent(state->original_accent);
    }
    if (state->screen == SETTINGS_SCREEN_BRIGHTNESS)
    {
        ui_preview_brightness(state->original_brightness);
    }
    ui_text_input_clear(&state->password);
}

static void show_network_list(SettingsState *state)
{
    state->screen = SETTINGS_SCREEN_NETWORKS;
    state->selected_network = 0;
    state->first_visible_network = 0;
    state->context->network->request_scan();
}

static bool request_connection(
    SettingsState *state,
    const char *password,
    NetworkAuth auth)
{
    if (!state->context->network->request_connect(
            state->pending_network.ssid,
            password,
            auth))
    {
        return false;
    }

    snprintf(
        state->requested_ssid,
        sizeof(state->requested_ssid),
        "%s",
        state->pending_network.ssid);
    state->connection_requested = true;
    state->screen = SETTINGS_SCREEN_STATUS;
    return true;
}

static void connect_selected_network(SettingsState *state)
{
    if (state->selected_network >= state->network.access_point_count)
    {
        return;
    }

    state->pending_network =
        state->network.access_points[state->selected_network];
    if (state->network.status == NETWORK_STATUS_CONNECTED &&
        strcmp(
            state->pending_network.ssid,
            state->network.connected_ssid) == 0)
    {
        state->connection_requested = false;
        state->screen = SETTINGS_SCREEN_STATUS;
        return;
    }
    if (state->pending_network.auth == NETWORK_AUTH_OPEN)
    {
        request_connection(state, "", NETWORK_AUTH_OPEN);
        return;
    }

    ui_text_input_clear(&state->password);
    state->password_too_short = false;
    state->screen = SETTINGS_SCREEN_PASSWORD;
}

static AppResult handle_status_event(
    SettingsState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        show_network_list(state);
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        state->screen = SETTINGS_SCREEN_MENU;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_A)
    {
        state->connection_requested = false;
        state->context->network->request_forget();
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_menu_event(
    SettingsState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_UP || event->button == INPUT_DOWN)
    {
        state->selected_menu = ui_list_move(
            state->selected_menu,
            SETTINGS_MENU_ITEM_COUNT,
            event->button == INPUT_UP ? -1 : 1);
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        if (state->selected_menu == SETTINGS_MENU_WIFI)
        {
            if ((state->context->available_capabilities & APP_CAP_WIFI) == 0)
            {
                return APP_RESULT_CONTINUE;
            }
            state->screen = SETTINGS_SCREEN_STATUS;
        }
        else if (state->selected_menu == SETTINGS_MENU_ACCENT)
        {
            state->original_accent = ui_accent();
            state->selected_accent_position = ui_accent();
            state->first_visible_accent = ui_list_first_visible(
                state->selected_accent_position,
                0,
                SETTINGS_VISIBLE_ACCENTS);
            state->screen = SETTINGS_SCREEN_ACCENTS;
        }
        else
        {
            state->original_brightness = ui_brightness();
            state->screen = SETTINGS_SCREEN_BRIGHTNESS;
        }
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        return APP_RESULT_HOME;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_accent_event(
    SettingsState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_UP || event->button == INPUT_DOWN)
    {
        state->selected_accent_position = ui_list_move(
            state->selected_accent_position,
            UI_ACCENT_COUNT,
            event->button == INPUT_UP ? -1 : 1);
        state->first_visible_accent = ui_list_first_visible(
            state->selected_accent_position,
            state->first_visible_accent,
            SETTINGS_VISIBLE_ACCENTS);
        ui_preview_accent((UiAccent)state->selected_accent_position);
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        ui_commit_accent((UiAccent)state->selected_accent_position);
        state->screen = SETTINGS_SCREEN_MENU;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        ui_preview_accent(state->original_accent);
        state->screen = SETTINGS_SCREEN_MENU;
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_brightness_event(
    SettingsState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_UP || event->button == INPUT_DOWN)
    {
        int brightness = ui_brightness();
        brightness += event->button == INPUT_UP
            ? UI_BRIGHTNESS_STEP
            : -UI_BRIGHTNESS_STEP;
        if (brightness < UI_BRIGHTNESS_MINIMUM)
        {
            brightness = UI_BRIGHTNESS_MINIMUM;
        }
        if (brightness > UI_BRIGHTNESS_MAXIMUM)
        {
            brightness = UI_BRIGHTNESS_MAXIMUM;
        }
        ui_preview_brightness((uint8_t)brightness);
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        ui_commit_brightness(ui_brightness());
        state->screen = SETTINGS_SCREEN_MENU;
        return APP_RESULT_REDRAW;
    }
    if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        ui_preview_brightness(state->original_brightness);
        state->screen = SETTINGS_SCREEN_MENU;
        return APP_RESULT_REDRAW;
    }
    return APP_RESULT_CONTINUE;
}

static AppResult handle_network_list_event(
    SettingsState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_UP)
    {
        state->selected_network = ui_list_move(
            state->selected_network,
            state->network.access_point_count,
            -1);
    }
    else if (event->button == INPUT_DOWN)
    {
        state->selected_network = ui_list_move(
            state->selected_network,
            state->network.access_point_count,
            1);
    }
    else if (event->button == INPUT_RIGHT || event->button == INPUT_CLICK)
    {
        connect_selected_network(state);
    }
    else if (event->button == INPUT_LEFT || event->button == INPUT_B)
    {
        state->screen = SETTINGS_SCREEN_STATUS;
    }
    else if (event->button == INPUT_A)
    {
        state->context->network->request_scan();
    }
    else
    {
        return APP_RESULT_CONTINUE;
    }

    state->first_visible_network = ui_list_first_visible(
        state->selected_network,
        state->first_visible_network,
        SETTINGS_VISIBLE_NETWORKS);
    return APP_RESULT_REDRAW;
}

static AppResult handle_password_event(
    SettingsState *state,
    const InputEvent *event)
{
    if (event->button == INPUT_B && state->password.length == 0)
    {
        state->screen = SETTINGS_SCREEN_NETWORKS;
        return APP_RESULT_REDRAW;
    }

    const UiTextInputResult result =
        ui_text_input_handle_event(&state->password, event);
    if (result == UI_TEXT_INPUT_SUBMIT)
    {
        if (state->password.length < 8)
        {
            state->password_too_short = true;
            return APP_RESULT_REDRAW;
        }
        if (request_connection(
                state,
                state->password.value,
                state->pending_network.auth))
        {
            ui_text_input_clear(&state->password);
        }
        return APP_RESULT_REDRAW;
    }
    if (result == UI_TEXT_INPUT_CHANGED)
    {
        state->password_too_short = false;
    }
    return result == UI_TEXT_INPUT_CHANGED
        ? APP_RESULT_REDRAW
        : APP_RESULT_CONTINUE;
}

static AppResult settings_handle_event(
    void *state_memory,
    const InputEvent *event)
{
    SettingsState *state = state_memory;
    if (event->action != INPUT_PRESSED && event->action != INPUT_REPEAT)
    {
        return APP_RESULT_CONTINUE;
    }

    switch (state->screen)
    {
        case SETTINGS_SCREEN_MENU:
            return handle_menu_event(state, event);
        case SETTINGS_SCREEN_STATUS:
            return handle_status_event(state, event);
        case SETTINGS_SCREEN_NETWORKS:
            return handle_network_list_event(state, event);
        case SETTINGS_SCREEN_PASSWORD:
            return handle_password_event(state, event);
        case SETTINGS_SCREEN_ACCENTS:
            return handle_accent_event(state, event);
        case SETTINGS_SCREEN_BRIGHTNESS:
            return handle_brightness_event(state, event);
    }
    return APP_RESULT_CONTINUE;
}

static AppResult settings_update(void *state_memory, uint32_t elapsed_ms)
{
    SettingsState *state = state_memory;
    state->refresh_elapsed_ms += elapsed_ms;
    if (state->refresh_elapsed_ms < SETTINGS_REFRESH_INTERVAL_MS)
    {
        return APP_RESULT_CONTINUE;
    }
    state->refresh_elapsed_ms %= SETTINGS_REFRESH_INTERVAL_MS;

    const uint32_t previous_revision = state->network.revision;
    refresh_snapshot(state);
    if (state->connection_requested)
    {
        const bool connected_to_requested =
            state->network.status == NETWORK_STATUS_CONNECTED &&
            strcmp(
                state->network.connected_ssid,
                state->requested_ssid) == 0;
        const bool connection_failed =
            state->network.status == NETWORK_STATUS_BAD_PASSWORD ||
            state->network.status == NETWORK_STATUS_NOT_FOUND ||
            state->network.status == NETWORK_STATUS_ERROR;
        if (connected_to_requested || connection_failed)
        {
            state->connection_requested = false;
        }
    }
    if (state->selected_network >= state->network.access_point_count)
    {
        state->selected_network = 0;
        state->first_visible_network = 0;
    }
    return state->network.revision != previous_revision
        ? APP_RESULT_REDRAW
        : APP_RESULT_CONTINUE;
}

static DisplayColor status_color(NetworkStatus status)
{
    switch (status)
    {
        case NETWORK_STATUS_CONNECTED:
            return ui_success_color();
        case NETWORK_STATUS_CONNECTING:
        case NETWORK_STATUS_SCANNING:
            return ui_activity_color();
        case NETWORK_STATUS_BAD_PASSWORD:
        case NETWORK_STATUS_NOT_FOUND:
        case NETWORK_STATUS_ERROR:
            return ui_error_color();
        default:
            return ui_chrome_color();
    }
}

static void render_status(
    const SettingsState *state,
    DisplaySurface *surface)
{
    const NetworkStatus visible_status = state->connection_requested
        ? NETWORK_STATUS_CONNECTING
        : state->network.status;
    const DisplayColor color = status_color(visible_status);
    display_draw_text_with_font(
        surface,
        10,
        37,
        network_status_name(visible_status),
        DISPLAY_FONT_MEDIUM,
        color,
        DISPLAY_COLOR_BLACK);

    const char *visible_ssid = state->connection_requested
        ? state->requested_ssid
        : state->network.connected_ssid;
    if (visible_ssid[0] != '\0')
    {
        display_draw_text(
            surface, 10, 68,
            visible_ssid,
            ui_neutral_text_color(), DISPLAY_COLOR_BLACK);
    }
    if (state->network.ip_address[0] != '\0')
    {
        display_draw_text(
            surface, 10, 87,
            state->network.ip_address,
            DISPLAY_COLOR_GRAY, DISPLAY_COLOR_BLACK);
    }
    ui_draw_footer(surface, ">/CLICK:NETS </B:BACK A:FORGET");
}

static void render_menu(
    const SettingsState *state,
    DisplaySurface *surface)
{
    const bool wifi_available =
        (state->context->available_capabilities & APP_CAP_WIFI) != 0;
    ui_draw_menu_item(
        surface,
        30,
        "Wi-Fi",
        wifi_available ? network_status_name(state->network.status) : "No radio",
        state->selected_menu == SETTINGS_MENU_WIFI,
        wifi_available);
    ui_draw_menu_item(
        surface,
        54,
        "Accent color",
        ui_accent_name(ui_accent()),
        state->selected_menu == SETTINGS_MENU_ACCENT,
        true);
    char brightness[8];
    snprintf(brightness, sizeof(brightness), "%u%%", ui_brightness());
    ui_draw_menu_item(
        surface,
        78,
        "Brightness",
        brightness,
        state->selected_menu == SETTINGS_MENU_BRIGHTNESS,
        true);
    ui_draw_footer(
        surface,
        "UP/DN:MOVE >/CLICK:OPEN B:HOME");
}

static void render_brightness(DisplaySurface *surface)
{
    const bool tall_display =
        surface->height >= SETTINGS_TALL_DISPLAY_HEIGHT;
    const uint16_t meter_top = tall_display ? 27 : 25;
    const uint16_t meter_step = tall_display ? 19 : 9;
    const uint16_t segment_height = tall_display ? 15 : 7;
    const uint16_t value_y = tall_display
        ? (uint16_t)(
            meter_top +
            (meter_step * 10 -
                display_font_height(DISPLAY_FONT_MEDIUM)) / 2)
        : 55;

    char value[8];
    snprintf(value, sizeof(value), "%u%%", ui_brightness());
    display_draw_text_with_font(
        surface,
        126,
        value_y,
        value,
        DISPLAY_FONT_MEDIUM,
        ui_menu_text_color(),
        DISPLAY_COLOR_BLACK);

    const uint8_t active_segments =
        ui_brightness() / UI_BRIGHTNESS_STEP;
    for (uint8_t i = 0; i < 10; i++)
    {
        const uint8_t level = (uint8_t)(10 - i);
        const uint16_t y = (uint16_t)(meter_top + i * meter_step);
        const bool active = level <= active_segments;
        display_draw_rectangle(
            surface,
            91,
            y,
            113,
            (uint16_t)(y + segment_height - 1),
            active ? ui_accent_color() : ui_chrome_color(),
            active ? 3 : 1);
    }
    ui_draw_footer(surface, "UP/DN:ADJUST CLICK:SAVE B:BACK");
}

static void render_accent_selection(
    const SettingsState *state,
    DisplaySurface *surface)
{
    const bool tall_display =
        surface->height >= SETTINGS_TALL_DISPLAY_HEIGHT;
    const size_t first = tall_display ? 0 : state->first_visible_accent;
    const size_t visible_count = tall_display
        ? UI_ACCENT_COUNT
        : SETTINGS_VISIBLE_ACCENTS;
    const size_t end = first + visible_count < UI_ACCENT_COUNT
        ? first + visible_count
        : UI_ACCENT_COUNT;
    uint16_t y = tall_display ? 29 : 26;
    const uint16_t row_height = tall_display ? 23 : 19;
    for (size_t i = first; i < end; i++)
    {
        const UiAccent accent = (UiAccent)i;
        ui_draw_menu_item(
            surface,
            y,
            ui_accent_name(accent),
            accent == state->original_accent ? "SAVED" : "",
            i == state->selected_accent_position,
            true);
        display_draw_rectangle(
            surface,
            surface->width - 60,
            y + 1,
            surface->width - 51,
            y + 10,
            ui_accent_palette_color(accent),
            4);
        y = (uint16_t)(y + row_height);
    }
    ui_draw_footer(surface, "UP/DN:PREVIEW CLICK:SAVE B:BACK");
}

static void render_network_list(
    const SettingsState *state,
    DisplaySurface *surface)
{
    if (state->network.status == NETWORK_STATUS_SCANNING &&
        state->network.access_point_count == 0)
    {
        display_draw_text_with_font(
            surface, 10, 48, "SCANNING...",
            DISPLAY_FONT_MEDIUM,
            ui_activity_color(),
            DISPLAY_COLOR_BLACK);
    }
    else if (state->network.access_point_count == 0)
    {
        display_draw_text(
            surface, 10, 48, "No networks found",
            ui_error_color(), DISPLAY_COLOR_BLACK);
    }
    else
    {
        const size_t end = state->first_visible_network +
            SETTINGS_VISIBLE_NETWORKS < state->network.access_point_count
            ? state->first_visible_network + SETTINGS_VISIBLE_NETWORKS
            : state->network.access_point_count;
        uint16_t y = 32;
        for (size_t i = state->first_visible_network; i < end; i++)
        {
            char detail[12];
            const bool connected =
                state->network.status == NETWORK_STATUS_CONNECTED &&
                strcmp(
                    state->network.access_points[i].ssid,
                    state->network.connected_ssid) == 0;
            if (connected)
            {
                snprintf(detail, sizeof(detail), "CONNECTED");
            }
            else
            {
                snprintf(
                    detail,
                    sizeof(detail),
                    "%s %d",
                    state->network.access_points[i].auth == NETWORK_AUTH_OPEN
                        ? "OPEN"
                        : "LOCK",
                    state->network.access_points[i].rssi);
            }
            ui_draw_list_item(
                surface,
                y,
                state->network.access_points[i].ssid,
                detail,
                i == state->selected_network);
            y += 21;
        }
    }
    ui_draw_footer(surface, ">/CLICK:OPEN </B:BACK A:SCAN");
}

static void render_password(
    const SettingsState *state,
    DisplaySurface *surface)
{
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "SSID: %.25s", state->pending_network.ssid);
    display_draw_text(
        surface, 5, 1, ssid,
        DISPLAY_COLOR_GRAY, DISPLAY_COLOR_BLACK);
    ui_draw_text_input(surface, &state->password, 14);
    ui_draw_virtual_keyboard(surface, &state->password, 38);
    ui_draw_footer(
        surface,
        state->password_too_short
            ? "PASSWORD MUST BE 8+ CHARACTERS"
            : "JOY MOVE CLICK ADD B DEL A ENTER");
}

static void settings_render(
    const void *state_memory,
    DisplaySurface *surface)
{
    const SettingsState *state = state_memory;
    display_clear(surface, DISPLAY_COLOR_BLACK);
    if (state->screen == SETTINGS_SCREEN_PASSWORD)
    {
        render_password(state, surface);
        return;
    }
    switch (state->screen)
    {
        case SETTINGS_SCREEN_MENU:
            ui_draw_header(surface, "SETTINGS");
            render_menu(state, surface);
            break;
        case SETTINGS_SCREEN_STATUS:
            ui_draw_header(surface, "WI-FI");
            render_status(state, surface);
            break;
        case SETTINGS_SCREEN_NETWORKS:
            ui_draw_header(surface, "NETWORKS");
            render_network_list(state, surface);
            break;
        case SETTINGS_SCREEN_PASSWORD:
            break;
        case SETTINGS_SCREEN_ACCENTS:
            ui_draw_header(surface, "ACCENT COLOR");
            render_accent_selection(state, surface);
            break;
        case SETTINGS_SCREEN_BRIGHTNESS:
            ui_draw_header(surface, "BRIGHTNESS");
            render_brightness(surface);
            break;
    }
}

const Application settings_application = {
    .name = "Settings",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(SettingsState),
    .state_policy = APP_STATE_TRANSIENT,
    .start = settings_start,
    .pause = NULL,
    .resume = NULL,
    .stop = settings_stop,
    .handle_event = settings_handle_event,
    .update = settings_update,
    .render = settings_render,
};
