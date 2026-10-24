#include "pico/bootrom.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "common/logger.h"
#include "app_manager.h"
#include "app_registry.h"
#include "input_events.h"
#include "launcher_app.h"
#include "network_service.h"
#include "platform/display.h"
#include "platform/input.h"
#include "platform/log.h"
#include "platform/temperature.h"
#include "platform/time.h"
#include "ui.h"

static int enter_usb_bootloader(void)
{
    reset_usb_boot(0, 0);
    return 0;
}

#define UI_TASK_STACK_DEPTH 2048
#define UI_TASK_PRIORITY 2

static StaticTask_t ui_task_storage;
static StackType_t ui_task_stack[UI_TASK_STACK_DEPTH];
static AppManager manager;
static DisplaySurface *surface;

static void ui_task(void *parameter)
{
    (void)parameter;
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
        if (display_present_complete() && app_manager_render(&manager))
        {
            display_present_async(surface);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void vApplicationMallocFailedHook(void)
{
    panic("FreeRTOS allocation failed");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    panic("FreeRTOS stack overflow: %s", task_name);
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t **task_buffer,
    StackType_t **stack_buffer,
    configSTACK_DEPTH_TYPE *stack_size)
{
    static StaticTask_t idle_task_storage;
    static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];

    *task_buffer = &idle_task_storage;
    *stack_buffer = idle_task_stack;
    *stack_size = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t **task_buffer,
    StackType_t **stack_buffer,
    configSTACK_DEPTH_TYPE *stack_size)
{
    static StaticTask_t timer_task_storage;
    static StackType_t timer_task_stack[configTIMER_TASK_STACK_DEPTH];

    *task_buffer = &timer_task_storage;
    *stack_buffer = timer_task_stack;
    *stack_size = configTIMER_TASK_STACK_DEPTH;
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

    ui_theme_init();

    platform_input_init();
    platform_temperature_init();

    surface = display_acquire_surface();
    uint32_t capabilities =
        APP_CAP_DISPLAY | APP_CAP_INPUT | APP_CAP_TEMPERATURE;
#if defined(CYW43_WL_GPIO_LED_PIN)
    capabilities |= APP_CAP_WIFI;
#endif

    if (!app_manager_init(
            &manager,
            surface,
            &launcher_application,
            application_registry,
            application_registry_count,
            capabilities,
            platform_temperature_read_c,
            network_service_api(),
            weather_service_api()))
    {
        LOG_ERROR("main", "application manager initialization failed");
        return enter_usb_bootloader();
    }

    if (!network_service_start())
    {
        LOG_ERROR("main", "network service task creation failed");
        return enter_usb_bootloader();
    }

    if (xTaskCreateStatic(
            ui_task,
            "ui",
            UI_TASK_STACK_DEPTH,
            NULL,
            UI_TASK_PRIORITY,
            ui_task_stack,
            &ui_task_storage) == NULL)
    {
        LOG_ERROR("main", "UI task creation failed");
        return enter_usb_bootloader();
    }

    LOG_INFO("main", "application firmware initialized; starting FreeRTOS");
    vTaskStartScheduler();
    return enter_usb_bootloader();
}
