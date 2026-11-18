if(APP_HARDWARE STREQUAL "waveshare_pico_lcd_1_14")
    set(APP_BOARD_SOURCES
        boards/waveshare_pico_lcd_1_14/display.c
        boards/waveshare_pico_lcd_1_14/input.c
        rp2040/rgb_led_none.c)
    set(APP_BOARD_LIBRARIES LCD)
elseif(APP_HARDWARE STREQUAL "spotpear_rpi_lcd_1_54")
    set(APP_BOARD_SOURCES
        boards/spotpear_rpi_lcd_1_54/display.c
        boards/spotpear_rpi_lcd_1_54/input.c
        rp2040/rgb_led_none.c)
    set(APP_BOARD_LIBRARIES)
elseif(APP_HARDWARE STREQUAL "geeekpi_ep0172")
    set(APP_BOARD_SOURCES
        boards/geeekpi_ep0172/display.c
        boards/geeekpi_ep0172/input.c
        boards/geeekpi_ep0172/touch.c
        boards/geeekpi_ep0172/rgb_led.c)
    set(APP_BOARD_LIBRARIES pico_status_led)
else()
    message(FATAL_ERROR "Unsupported APP_HARDWARE: ${APP_HARDWARE}")
endif()

add_library(app_platform STATIC
    display_primitives.c
    network.c
    ssh.c
    weather.c
    ${APP_BOARD_SOURCES}
    rp2040/log.c
    rp2040/network_credentials.c
    rp2040/preferences.c
    rp2040/spi_dma.c
    rp2040/temperature.c
    rp2040/time.c
)

target_include_directories(app_platform
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_SOURCE_DIR}/third_party/waveshare/Config
        ${CMAKE_SOURCE_DIR}/third_party/waveshare/Fonts
        ${CMAKE_SOURCE_DIR}/third_party/waveshare/GUI
        ${CMAKE_SOURCE_DIR}/third_party/waveshare/LCD
)

target_link_libraries(app_platform
    PUBLIC
        pico_stdlib
        project_logger
    PRIVATE
        Config
        Fonts
        GUI
        ${APP_BOARD_LIBRARIES}
        hardware_adc
        hardware_dma
        hardware_flash
        hardware_i2c
        hardware_spi
        pico_flash
)

if(APP_HARDWARE STREQUAL "geeekpi_ep0172")
    target_compile_definitions(app_platform PRIVATE
        PICO_DEFAULT_WS2812_PIN=12
        PICO_STATUS_LED_AVAILABLE=0
        PICO_COLORED_STATUS_LED_AVAILABLE=1)
endif()
