add_executable(pico-apps
    main.c
    system/app_registry.c
)

pico_set_program_name(pico-apps "pico-apps")
pico_set_program_version(pico-apps "0.1")

pico_enable_stdio_uart(pico-apps 0)
pico_enable_stdio_usb(pico-apps 1)

target_link_libraries(pico-apps
    PRIVATE
        pico_stdlib
        FreeRTOS-Kernel-Heap4
        app_platform
        app_framework
        app_ui
        calculator_app
        launcher_app
        network_service
        settings_app
        ssh_app
        ssh_service
        snake_app
        temperature_app
        weather_app
)

pico_add_extra_outputs(pico-apps)
