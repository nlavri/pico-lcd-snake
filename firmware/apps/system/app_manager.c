#include "app_manager.h"

#include <string.h>

#include "common/logger.h"

#define LOG_COMPONENT "apps"

static size_t aligned_state_size(size_t size)
{
    const size_t alignment = _Alignof(max_align_t);
    return (size + alignment - 1) / alignment * alignment;
}

static void request_launch(void *context, size_t application_index)
{
    AppManager *manager = context;
    manager->launch_requested = true;
    manager->restart_requested = false;
    manager->requested_application_index = application_index;
}

static void request_restart(void *context, size_t application_index)
{
    AppManager *manager = context;
    manager->launch_requested = true;
    manager->restart_requested = true;
    manager->requested_application_index = application_index;
}

static void deactivate(ManagedApplication *slot)
{
    if (slot == NULL || !slot->initialized)
    {
        return;
    }

    const Application *application = slot->application;
    if (application->state_policy == APP_STATE_PERSISTENT)
    {
        if (application->pause != NULL)
        {
            application->pause(slot->state);
        }
        return;
    }

    if (application->stop != NULL)
    {
        application->stop(slot->state);
    }
    memset(slot->state, 0, application->state_size);
    slot->initialized = false;
}

static bool activate(
    AppManager *manager,
    ManagedApplication *slot,
    bool restart)
{
    const Application *application = slot->application;
    const Application *previous_application = manager->active_slot == NULL
        ? NULL
        : manager->active_slot->application;
    const bool was_initialized = slot->initialized;

    if (!app_is_available(
            application,
            manager->context.available_capabilities))
    {
        LOG_WARNING(
            LOG_COMPONENT,
            "cannot open %s: required capability unavailable",
            application->name);
        return false;
    }

    if (manager->active_slot != slot)
    {
        deactivate(manager->active_slot);
        manager->active_slot = NULL;
    }

    if (restart && slot->initialized)
    {
        if (application->stop != NULL)
        {
            application->stop(slot->state);
        }
        slot->initialized = false;
    }

    if (!slot->initialized)
    {
        memset(slot->state, 0, application->state_size);
        if (application->start != NULL &&
            !application->start(slot->state, &manager->context))
        {
            LOG_ERROR(
                LOG_COMPONENT,
                "failed to start %s",
                application->name);
            return false;
        }
        slot->initialized = true;
    }
    else if (application->resume != NULL)
    {
        application->resume(slot->state);
    }

    manager->active_slot = slot;
    manager->redraw_requested = true;

    const char *action = restart
        ? "restart"
        : (was_initialized ? "resume" : "start");
    if (previous_application == NULL)
    {
        LOG_INFO(LOG_COMPONENT, "%s %s", action, application->name);
    }
    else
    {
        LOG_INFO(
            LOG_COMPONENT,
            "%s -> %s (%s)",
            previous_application->name,
            application->name,
            action);
    }
    return true;
}

static void return_to_launcher(AppManager *manager)
{
    if (manager->active_slot == &manager->launcher_slot)
    {
        return;
    }

    if (!activate(manager, &manager->launcher_slot, false))
    {
        manager->active_slot = NULL;
    }
}

static void process_result(AppManager *manager, AppResult result)
{
    if (manager->launch_requested)
    {
        const size_t index = manager->requested_application_index;
        const bool restart = manager->restart_requested;
        manager->launch_requested = false;
        manager->restart_requested = false;

        if (index < manager->context.application_count &&
            !activate(manager, &manager->application_slots[index], restart))
        {
            return_to_launcher(manager);
        }
        return;
    }

    if (result == APP_RESULT_HOME)
    {
        return_to_launcher(manager);
    }
    else if (result == APP_RESULT_REDRAW)
    {
        manager->redraw_requested = true;
    }
}

static bool assign_state_slots(
    AppManager *manager,
    const Application *launcher,
    const Application *const *applications,
    size_t application_count)
{
    if (application_count > APP_MANAGER_MAX_APPLICATIONS)
    {
        return false;
    }

    size_t offset = 0;
    manager->launcher_slot = (ManagedApplication){
        .application = launcher,
        .state = &manager->state_pool.bytes[offset],
    };
    offset += aligned_state_size(launcher->state_size);
    if (offset > sizeof(manager->state_pool.bytes))
    {
        return false;
    }

    for (size_t i = 0; i < application_count; i++)
    {
        if (applications[i] == NULL)
        {
            return false;
        }

        const size_t slot_size = aligned_state_size(applications[i]->state_size);
        if (offset + slot_size > sizeof(manager->state_pool.bytes))
        {
            return false;
        }

        manager->application_slots[i] = (ManagedApplication){
            .application = applications[i],
            .state = &manager->state_pool.bytes[offset],
        };
        offset += slot_size;
    }

    return offset <= sizeof(manager->state_pool.bytes);
}

bool app_manager_init(
    AppManager *manager,
    DisplaySurface *surface,
    const Application *launcher,
    const Application *const *applications,
    size_t application_count,
    uint32_t available_capabilities,
    float (*read_temperature_c)(void),
    const NetworkServiceApi *network,
    const SshServiceApi *ssh,
    const WeatherServiceApi *weather)
{
    if (manager == NULL || surface == NULL || launcher == NULL ||
        applications == NULL)
    {
        return false;
    }

    *manager = (AppManager){0};
    manager->surface = surface;
    manager->context = (AppContext){
        .applications = applications,
        .application_count = application_count,
        .available_capabilities = available_capabilities,
        .display_width = surface->width,
        .display_height = surface->height,
        .read_temperature_c = read_temperature_c,
        .network = network,
        .ssh = ssh,
        .weather = weather,
        .request_launch = request_launch,
        .request_restart = request_restart,
        .manager = manager,
    };

    if (!assign_state_slots(manager, launcher, applications, application_count))
    {
        return false;
    }

    return activate(manager, &manager->launcher_slot, false);
}

void app_manager_handle_event(AppManager *manager, const InputEvent *event)
{
    if (event->button == INPUT_HOME && event->action == INPUT_PRESSED)
    {
        return_to_launcher(manager);
        return;
    }

    if (manager->active_slot != NULL &&
        manager->active_slot->application->handle_event != NULL)
    {
        process_result(
            manager,
            manager->active_slot->application->handle_event(
                manager->active_slot->state,
                event));
    }
}

void app_manager_update(AppManager *manager, uint32_t elapsed_ms)
{
    if (manager->active_slot != NULL &&
        manager->active_slot->application->update != NULL)
    {
        process_result(
            manager,
            manager->active_slot->application->update(
                manager->active_slot->state,
                elapsed_ms));
    }
}

bool app_manager_render(AppManager *manager)
{
    if (!manager->redraw_requested || manager->active_slot == NULL ||
        manager->active_slot->application->render == NULL)
    {
        return false;
    }

    manager->active_slot->application->render(
        manager->active_slot->state,
        manager->surface);
    manager->redraw_requested = false;
    return true;
}

const Application *app_manager_active_application(const AppManager *manager)
{
    return manager->active_slot == NULL
        ? NULL
        : manager->active_slot->application;
}
