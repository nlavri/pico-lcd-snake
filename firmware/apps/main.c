#include "pico/bootrom.h"
#include "pico/stdlib.h"

#include "common/logger.h"
#include "app_manager.h"
#include "app_registry.h"
#include "input_events.h"
#include "launcher_app.h"
#include "platform/display.h"
#include "platform/input.h"
#include "platform/log.h"
#include "platform/temperature.h"
#include "platform/time.h"

static int enter_usb_bootloader(void)
{
    reset_usb_boot(0, 0);
    return 0;
}

int main(void)
{
    stdio_init_all();
    platform_log_init();

    if (!display_init())
    {
        LOG_ERROR("main", "display initialization failed");
        return enter_usb_bootloader();
    }

    platform_input_init();
    platform_temperature_init();

    DisplaySurface *surface = display_acquire_surface();
    static AppManager manager;
    const uint32_t capabilities =
        APP_CAP_DISPLAY | APP_CAP_INPUT | APP_CAP_TEMPERATURE;

    if (!app_manager_init(
            &manager,
            surface,
            &launcher_application,
            application_registry,
            application_registry_count,
            capabilities,
            platform_temperature_read_c))
    {
        LOG_ERROR("main", "application manager initialization failed");
        return enter_usb_bootloader();
    }

    LOG_INFO("main", "application firmware initialized");

    InputEventSource input_source = {0};
    uint32_t previous_time_ms = platform_time_ms();

    while (true)
    {
        const uint32_t now_ms = platform_time_ms();
        const uint32_t elapsed_ms = now_ms - previous_time_ms;
        previous_time_ms = now_ms;

        const PlatformInputState input = platform_input_read();
        InputEvent events[INPUT_EVENT_BATCH_CAPACITY];
        const size_t event_count = input_events_update(
            &input_source,
            &input,
            now_ms,
            events);

        for (size_t i = 0; i < event_count; i++)
        {
            app_manager_handle_event(&manager, &events[i]);
        }

        app_manager_update(&manager, elapsed_ms);
        if (app_manager_render(&manager))
        {
            display_present(surface);
        }

        platform_sleep_ms(5);
    }
}
