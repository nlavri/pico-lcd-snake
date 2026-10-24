if(APP_HARDWARE STREQUAL "waveshare_pico_lcd_1_14")
    set(APP_BOARD_SOURCES
        boards/waveshare_pico_lcd_1_14/display.c
        boards/waveshare_pico_lcd_1_14/input.c)
    set(APP_BOARD_LIBRARIES LCD)
elseif(APP_HARDWARE STREQUAL "spotpear_rpi_lcd_1_54")
    set(APP_BOARD_SOURCES
        boards/spotpear_rpi_lcd_1_54/display.c
        boards/spotpear_rpi_lcd_1_54/input.c)
    set(APP_BOARD_LIBRARIES)
else()
    message(FATAL_ERROR "Unsupported APP_HARDWARE: ${APP_HARDWARE}")
endif()

add_library(app_platform STATIC
    network.c
    weather.c
    ${APP_BOARD_SOURCES}
    rp2040/log.c
    rp2040/network_credentials.c
    rp2040/preferences.c
    rp2040/temperature.c
    rp2040/time.c
)

target_include_directories(app_platform
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
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
        hardware_flash
        hardware_spi
        pico_flash
)
