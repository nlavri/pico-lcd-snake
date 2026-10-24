#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_manager.h"

typedef struct TestState
{
    AppContext *context;
    uint32_t value;
} TestState;

static unsigned app_start_count;
static unsigned app_pause_count;
static unsigned app_resume_count;
static unsigned app_stop_count;
static bool app_was_cleared_before_start;
static TestState *app_state;

static bool launcher_start(void *state, AppContext *context)
{
    TestState *launcher = state;
    launcher->context = context;
    return true;
}

static AppResult launcher_handle_event(void *state, const InputEvent *event)
{
    TestState *launcher = state;
    if (event->action != INPUT_PRESSED)
    {
        return APP_RESULT_CONTINUE;
    }

    if (event->button == INPUT_A)
    {
        launcher->context->request_launch(launcher->context->manager, 0);
    }
    else if (event->button == INPUT_B)
    {
        launcher->context->request_restart(launcher->context->manager, 0);
    }
    return APP_RESULT_CONTINUE;
}

static bool test_app_start(void *state, AppContext *context)
{
    (void)context;
    app_state = state;
    app_was_cleared_before_start = app_state->value == 0;
    app_state->value = 42;
    app_start_count++;
    return true;
}

static void test_app_pause(void *state)
{
    (void)state;
    app_pause_count++;
}

static void test_app_resume(void *state)
{
    (void)state;
    app_resume_count++;
}

static void test_app_stop(void *state)
{
    (void)state;
    app_stop_count++;
}

static const Application launcher = {
    .name = "Launcher",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(TestState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = launcher_start,
    .handle_event = launcher_handle_event,
};

static const Application test_app = {
    .name = "Test",
    .required_capabilities = APP_CAP_DISPLAY | APP_CAP_INPUT,
    .state_size = sizeof(TestState),
    .state_policy = APP_STATE_PERSISTENT,
    .start = test_app_start,
    .pause = test_app_pause,
    .resume = test_app_resume,
    .stop = test_app_stop,
};

static void send_button(
    AppManager *manager,
    InputButton button)
{
    const InputEvent event = {
        .button = button,
        .action = INPUT_PRESSED,
    };
    app_manager_handle_event(manager, &event);
}

int main(void)
{
    uint16_t pixel = 0;
    DisplaySurface surface = {
        .pixels = &pixel,
        .width = 240,
        .height = 135,
    };
    const Application *const applications[] = {&test_app};
    AppManager manager;

    assert(app_manager_init(
        &manager,
        &surface,
        &launcher,
        applications,
        1,
        APP_CAP_DISPLAY | APP_CAP_INPUT,
        NULL,
        NULL,
        NULL,
        NULL));
    assert(app_manager_active_application(&manager) == &launcher);

    send_button(&manager, INPUT_A);
    assert(app_manager_active_application(&manager) == &test_app);
    assert(app_start_count == 1);
    assert(app_was_cleared_before_start);
    app_state->value = 99;

    send_button(&manager, INPUT_HOME);
    assert(app_manager_active_application(&manager) == &launcher);
    assert(app_pause_count == 1);

    send_button(&manager, INPUT_A);
    assert(app_manager_active_application(&manager) == &test_app);
    assert(app_resume_count == 1);
    assert(app_state->value == 99);

    send_button(&manager, INPUT_HOME);
    send_button(&manager, INPUT_B);
    assert(app_manager_active_application(&manager) == &test_app);
    assert(app_stop_count == 1);
    assert(app_start_count == 2);
    assert(app_was_cleared_before_start);
    assert(app_state->value == 42);

    return 0;
}
