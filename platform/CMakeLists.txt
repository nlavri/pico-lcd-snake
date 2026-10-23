add_library(app_platform STATIC
    boards/waveshare_pico_lcd_1_14/display.c
    boards/waveshare_pico_lcd_1_14/input.c
    rp2040/log.c
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
        LCD
        hardware_adc
        hardware_spi
)
