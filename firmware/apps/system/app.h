#ifndef PICO_LCD_SYSTEM_APP_H
#define PICO_LCD_SYSTEM_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/display.h"

typedef enum AppCapability
{
    APP_CAP_DISPLAY = 1u << 0,
    APP_CAP_INPUT = 1u << 1,
    APP_CAP_TEMPERATURE = 1u << 2,
    APP_CAP_WIFI = 1u << 3,
} AppCapability;

typedef enum InputButton
{
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CLICK,
    INPUT_A,
    INPUT_B,
    INPUT_HOME,
} InputButton;

typedef enum InputAction
{
    INPUT_PRESSED,
    INPUT_RELEASED,
    INPUT_REPEAT,
} InputAction;

typedef struct InputEvent
{
    InputButton button;
    InputAction action;
    uint32_t timestamp_ms;
} InputEvent;

typedef enum AppResult
{
    APP_RESULT_CONTINUE,
    APP_RESULT_REDRAW,
    APP_RESULT_HOME,
} AppResult;

typedef enum AppStatePolicy
{
    APP_STATE_PERSISTENT,
    APP_STATE_TRANSIENT,
} AppStatePolicy;

typedef struct Application Application;

typedef struct AppContext
{
    const Application *const *applications;
    size_t application_count;
    uint32_t available_capabilities;
    uint16_t display_width;
    uint16_t display_height;

    float (*read_temperature_c)(void);
    void (*request_launch)(void *manager, size_t application_index);
    void (*request_restart)(void *manager, size_t application_index);
    void *manager;
} AppContext;

struct Application
{
    const char *name;
    uint32_t required_capabilities;
    size_t state_size;
    AppStatePolicy state_policy;

    bool (*start)(void *state, AppContext *context);
    void (*pause)(void *state);
    void (*resume)(void *state);
    void (*stop)(void *state);
    AppResult (*handle_event)(void *state, const InputEvent *event);
    AppResult (*update)(void *state, uint32_t elapsed_ms);
    void (*render)(const void *state, DisplaySurface *surface);
};

static inline bool app_is_available(
    const Application *application,
    uint32_t available_capabilities)
{
    return (application->required_capabilities & ~available_capabilities) == 0;
}

#endif
