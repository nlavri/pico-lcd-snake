#ifndef PICO_LCD_SYSTEM_APP_MANAGER_H
#define PICO_LCD_SYSTEM_APP_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"

#define APP_STATE_POOL_SIZE 4096
#define APP_MANAGER_MAX_APPLICATIONS 8

typedef union AppStatePool
{
    max_align_t alignment;
    uint8_t bytes[APP_STATE_POOL_SIZE];
} AppStatePool;

typedef struct ManagedApplication
{
    const Application *application;
    void *state;
    bool initialized;
} ManagedApplication;

typedef struct AppManager
{
    AppStatePool state_pool;
    ManagedApplication launcher_slot;
    ManagedApplication application_slots[APP_MANAGER_MAX_APPLICATIONS];
    AppContext context;
    DisplaySurface *surface;
    ManagedApplication *active_slot;
    bool redraw_requested;
    bool launch_requested;
    bool restart_requested;
    size_t requested_application_index;
} AppManager;

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
    const WeatherServiceApi *weather);

void app_manager_handle_event(AppManager *manager, const InputEvent *event);
void app_manager_update(AppManager *manager, uint32_t elapsed_ms);
bool app_manager_render(AppManager *manager);
const Application *app_manager_active_application(const AppManager *manager);

#endif
