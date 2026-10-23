# Firmware Architecture

This document describes the architecture for evolving the Snake prototype into
a small application platform for Raspberry Pi Pico and Pico W. The repository
already uses the application, platform, firmware-product, and third-party
boundaries below. The callback application manager, registry, launcher, and
standalone DOOM target are implemented. FreeRTOS services are the next
implementation stage.

## Goals

- Run a launcher and several foreground applications from one firmware image.
- Use the official RP2040 FreeRTOS port for genuinely concurrent services.
- Keep application and game logic independent of FreeRTOS and RP2040 headers.
- Support both the original Pico and Pico W from the same application sources.
- Keep DOOM as a separate firmware product with its own memory and build rules.
- Make hardware-specific code replaceable without restructuring applications.
- Prefer fixed, measurable memory usage over hidden dynamic allocation.

FreeRTOS is not used as an application loader. It schedules the input, display,
and network services. The active foreground application is a callback-driven
component executed by a single UI task.

## Firmware Products

The repository produces two independent firmware products. The application
firmware has separate board variants.

| Preset | Board | Purpose | Output directory |
| --- | --- | --- | --- |
| `apps-pico-debug` | Pico | Launcher and local applications | `build/apps-pico/` |
| `apps-picow-debug` | Pico W | Launcher, local applications, and Wi-Fi services | `build/apps-picow/` |
| `doom-pico-release` | Pico | Specialized standalone DOOM firmware | `build/doom-pico/` |

The DOOM target uses a release-oriented configuration because its code size,
flash placement, clock settings, and memory layout differ from the application
firmware. It is not linked into the FreeRTOS image initially.

## Target Repository Layout

```text
pico-lcd-snake/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
│
├── common/
│   ├── logger.c
│   └── include/common/logger.h
│
├── cmake/
│   ├── pico_sdk_import.cmake
│   └── FreeRTOS_Kernel_import.cmake
│
├── firmware/
│   ├── apps/
│   │   ├── CMakeLists.txt
│   │   ├── main.c
│   │   ├── FreeRTOSConfig.h
│   │   └── system/
│   │       ├── app.h
│   │       ├── app_manager.c
│   │       ├── app_manager.h
│   │       ├── app_registry.c
│   │       ├── app_registry.h
│   │       └── services/
│   │           ├── input_service.c
│   │           ├── input_service.h
│   │           ├── display_service.c
│   │           ├── display_service.h
│   │           ├── network_service.c
│   │           └── network_service.h
│   │
│   └── doom/
│       ├── CMakeLists.txt
│       ├── main.c
│       ├── doom_board_config.h
│       └── README.md
│
├── apps/
│   ├── launcher/
│   │   ├── launcher_app.c
│   │   └── launcher_app.h
│   ├── snake/
│   │   ├── snake_app.c
│   │   ├── snake_app.h
│   │   ├── snake_game.c
│   │   └── snake_game.h
│   └── temperature/
│       ├── temperature_app.c
│       └── temperature_app.h
│
├── platform/
│   ├── include/platform/
│   │   ├── board.h
│   │   ├── display.h
│   │   ├── input.h
│   │   ├── network.h
│   │   ├── temperature.h
│   │   └── time.h
│   ├── rp2040/
│   │   ├── log.c
│   │   ├── platform_init.c
│   │   ├── temperature_rp2040.c
│   │   └── time_rp2040.c
│   └── boards/waveshare_pico_lcd_1_14/
│       ├── board.c
│       ├── board.h
│       ├── display.c
│       └── input.c
│
├── drivers/
│   ├── st7789/
│   │   ├── st7789.c
│   │   └── st7789.h
│   └── cyw43/
│       ├── network_cyw43.c
│       └── network_cyw43.h
│
├── third_party/
│   ├── FreeRTOS-Kernel/
│   ├── waveshare/
│   └── rp2040-doom-PicoLCD/
│
└── tests/
    ├── app_manager_tests.c
    ├── input_tests.c
    └── snake_game_tests.c
```

## Runtime Model

```text
                         FreeRTOS

 Input service task ── InputEvent queue ──┐
                                          ▼
                                    UI task
                              ┌───────────┴───────────┐
                              │ Application manager  │
                              │ One active app       │
                              └───────────┬───────────┘
                                          │ completed surface
                                          ▼
                                   Display service task
                                          │
                                          ▼
                                      ST7789 LCD

 Network service task ── responses ──────▶ UI task       Pico W only
```

FreeRTOS initially runs on core 0. Core 1 remains available for later display,
audio, or other measured performance work. Moving work to core 1 is a future
optimization, not part of the first migration.

## Application Model

Foreground applications are not permanent FreeRTOS tasks. They are passive
components invoked by the UI task through a common interface.

```c
typedef enum AppResult
{
    APP_RESULT_CONTINUE,
    APP_RESULT_REDRAW,
    APP_RESULT_HOME
} AppResult;

typedef struct Application
{
    const char *name;
    uint32_t required_capabilities;
    size_t state_size;
    AppStatePolicy state_policy;

    bool (*start)(void *state, AppContext *context);
    void (*pause)(void *state);
    void (*resume)(void *state);
    void (*stop)(void *state);

    AppResult (*handle_event)(
        void *state,
        const InputEvent *event);

    AppResult (*update)(
        void *state,
        uint32_t elapsed_ms);

    void (*render)(
        const void *state,
        DisplaySurface *surface);
} Application;
```

FreeRTOS types such as `TaskHandle_t`, `QueueHandle_t`, and `TickType_t` do not
appear in this interface. This keeps application code host-testable and makes a
future non-FreeRTOS runtime possible without changing application logic.

### Registry and Capabilities

Applications are compiled into a static registry:

```c
const Application *const application_registry[] = {
    &snake_application,
    &temperature_application,
};
```

Each application declares its required platform capabilities. The launcher
hides or disables applications whose requirements are unavailable. Examples
include:

```c
APP_CAP_DISPLAY
APP_CAP_INPUT
APP_CAP_TEMPERATURE
APP_CAP_WIFI
```

A weather application can therefore be included in both builds while only
being offered by the Pico W launcher.

### Application State

The manager owns one fixed, correctly aligned state pool. Each registered
application receives a stable slice sized from its `state_size`:

```c
#define APP_STATE_POOL_SIZE 8192

static max_align_t app_state_pool[
    APP_STATE_POOL_SIZE / sizeof(max_align_t)
];
```

Persistent applications are paused when they lose focus and resumed with their
state intact. Restart explicitly calls `stop()`, clears that application's
slice, and calls `start()` again. Transient applications are stopped and
cleared whenever they lose focus. Startup rejects a registry whose aligned
states do not fit in the pool.

### Switching Lifecycle

An application switch is coordinated by the UI task:

```text
Receive switch request
        ↓
Pause persistent app or stop transient app
        ↓
Wait for any submitted display transfer
        ↓
Discard queued foreground input
        ↓
Select the destination application's state slice
        ↓
Call start() once, or resume() when already initialized
        ↓
Render and submit its first frame
```

The application manager never switches applications from an interrupt or from
inside a service task.

## Service Responsibilities

### Logging

`common/logger` provides project-wide levelled logging without depending on
the Pico SDK, USB, UART, or FreeRTOS. Callers use `LOG_DEBUG`, `LOG_INFO`,
`LOG_WARNING`, and `LOG_ERROR` with a component name. The logger formats into a
fixed-size stack buffer and synchronously sends the result to an installed
`LogSink`; it performs no dynamic allocation.

The RP2040 firmware installs the current USB/UART `stdio` sink through
`platform_log_init()`. A different firmware product or a host test can replace
that sink with a UART writer, an RTOS queue, a ring buffer, a file capture, or
`NULL` for silent logging without changing callers. A future concurrent sink
is responsible for serializing output from multiple tasks.

### UI Task and Application Manager

- Own the active application and persistent state pool.
- Dispatch input events to the active application.
- Call `update()` using elapsed milliseconds rather than RTOS tick types.
- Request a display surface, call `render()`, and submit the completed surface.
- Process launcher requests and the global return-to-launcher action.
- Ensure only one foreground application is running at a time.
- Never access GPIO, SPI, CYW43, or the ADC directly.

### Input Service Task

- Be the only task that reads the joystick and button GPIOs.
- Sample and debounce up, down, left, right, joystick click, A, and B.
- Generate pressed, released, and optional repeat events.
- Detect the system-level A+B hold gesture.
- Send events to the UI task through a bounded FreeRTOS queue.
- Define and report an explicit overflow policy rather than silently blocking.

The application manager intercepts the system gesture before dispatching input
to the active application. Joystick click remains available to applications.

### Display Service Task

- Be the only task that sends commands or pixels to the physical LCD.
- Own the framebuffer and its transfer state.
- Give the UI task exclusive access to an available `DisplaySurface`.
- Transfer submitted surfaces using blocking SPI initially.
- Notify the UI task when the surface becomes available again.
- Allow a future DMA implementation without changing application code.

One RGB565 framebuffer is used initially. The UI task cannot render into it
while the display task is transferring it. This ownership protocol saves the
64,800 bytes that a second 240 x 135 RGB565 buffer would require.

### Network Service Task

The network service is only compiled when the selected board provides Wi-Fi.

- Own CYW43 and lwIP operations.
- Establish and monitor the Wi-Fi connection.
- Handle DNS, NTP, HTTP, and socket operations.
- Accept bounded requests from applications.
- Return completion, error, and cancellation events asynchronously.
- Keep network delays and reconnect attempts out of the UI task.

Applications do not call CYW43 or lwIP directly. Network configuration and
credentials are system concerns, not application state.

## Input Events

The platform-independent input model represents both the control and its
transition:

```c
typedef enum InputButton
{
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CLICK,
    INPUT_A,
    INPUT_B,
    INPUT_HOME
} InputButton;

typedef enum InputAction
{
    INPUT_PRESSED,
    INPUT_RELEASED,
    INPUT_REPEAT
} InputAction;
```

Launcher controls are:

- Up/down: change selection.
- A: start or resume the selected application.
- B: clear and restart the selected application.
- Joystick click: show or hide launcher help.
- Hold A+B: return to the launcher from any foreground application.

## Display and Board Boundaries

The Waveshare Pico LCD 1.14-inch product contains a 135 x 240 LCD panel driven
by an ST7789 controller. The application uses it in a 240 x 135 landscape
orientation.

```text
Application renderer
        ↓
platform/display.h
        ↓
Display service
        ↓
Waveshare board adapter
        ↓
ST7789 controller driver
        ↓
RP2040 SPI peripheral
```

The ST7789 driver is responsible for controller commands, address windows,
pixel format, and pixel transfer. The Waveshare board adapter is responsible
for GPIO assignments, panel dimensions and offsets, rotation, backlight,
joystick, and buttons.

The existing Waveshare `LCD_1in14` implementation combines these concerns. It
will first be wrapped behind the platform API and then split only where doing
so benefits testing, DMA support, or reuse.

## Memory and Concurrency Rules

- Use the official RP2040 FreeRTOS port.
- Prefer static allocation for tasks, queues, timers, and application state.
- Record a stack high-water mark for every service task during development.
- Avoid application-owned global hardware state.
- Do not call a blocking network operation from the UI task.
- Do not suspend or delete a task while it may own a resource.
- Do not access the framebuffer without acquiring it from the display service.
- Do not access the physical input GPIOs outside the input service.
- Treat queue capacity and overflow behavior as part of each service contract.
- Remove the current full-screen temporary stack allocation before starting
  the RTOS scheduler.

## Snake Boundaries

Snake is divided into a pure engine and an application adapter:

```text
snake_app.c
├── Application callbacks
├── Input-event translation
└── Display rendering

snake_game.c
├── Snake movement and growth
├── Direction validation
├── Fruit placement
├── Collision rules
└── Score and game state
```

`snake_game.c` must not include FreeRTOS, Pico SDK, Waveshare, or ST7789
headers. Its behavior should be testable as a normal host executable.

## DOOM Boundary

DOOM is a separate firmware image based on the vendored
`akeeh/rp2040-doom-PicoLCD` port for the Waveshare Pico LCD 1.14-inch board.

- It has its own `main()` and build configuration.
- It may use a custom linker script and fixed flash addresses.
- It may use a different SDK/toolchain version while the port is modernized.
- It does not use the application state pool or normal display service.
- The WAD or generated WHX asset is supplied separately and is not treated as
  a normal application resource.

An eventual launcher-to-DOOM transition would be a controlled reboot or
multi-image boot operation, not an in-process callback switch. That mechanism
is outside the initial application-platform scope.

## Migration from the Prototype

| Current source | Target responsibility |
| --- | --- |
| `game.c`, `game.h` | `apps/snake/snake_game.c`, `snake_game.h` |
| Snake drawing in `output_hal.c` | `apps/snake/snake_app.c` |
| LCD ownership in `output_hal.c` | Display service and board adapter |
| `input_hal.c`, `input_hal.h` | Waveshare input driver and input service |
| `runtime.c`, `runtime.h` | RP2040 time/platform implementation |
| `main.c` | Replaced by the selected firmware entry point |
| `lib/` vendor sources | `third_party/waveshare/` |

The migration should remain buildable after each phase:

1. Remove large temporary stack allocations and make framebuffer ownership
   explicit.
2. Introduce platform display, input, time, and temperature interfaces.
3. Split Snake into a pure engine and callback application adapter.
4. Add the launcher, registry, state pool, and bare application lifecycle.
   **Complete.**
5. Add the official RP2040 FreeRTOS port and service tasks.
6. Add the Pico W network service and capability-filtered applications.
7. Add the independent DOOM build using the board-specific upstream port.
   **Complete.**

## Architectural Decision Summary

- **Services are tasks** because they own concurrent hardware or blocking work.
- **Foreground applications are callbacks** because they share one screen and
  input focus, need uniform lifecycle control, and do not require independent
  scheduling.
- **Game engines are plain C modules** so rules remain portable and testable.
- **Hardware ownership is exclusive** to prevent races and hidden coupling.
- **Pico and Pico W share applications** through capability-based registration.
- **DOOM is a separate product** because its optimized runtime has fundamentally
  different memory, flash, and performance requirements.
