cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Raspberry Pi Pico VS Code extension integration.
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.vscode/extensions/raspberry-pi.raspberry-pi-pico-0.23.0/scripts/pico-vscode.cmake)
if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico CACHE STRING "Board type")
set(APP_HARDWARE "waveshare_pico_lcd_1_14" CACHE STRING
    "Application display/input hardware")
set_property(CACHE APP_HARDWARE PROPERTY STRINGS
    waveshare_pico_lcd_1_14
    spotpear_rpi_lcd_1_54)

include(cmake/pico_sdk_import.cmake)

project(pico-lcd C CXX ASM)

pico_sdk_init()

# Application firmware runs FreeRTOS on core 0 only; core 1 never executes
# from flash, so flash_safe_execute only needs to mask core-0 interrupts.
add_compile_definitions(PICO_FLASH_ASSUME_CORE1_SAFE=1)

set(FREERTOS_KERNEL_PATH
    ${CMAKE_CURRENT_LIST_DIR}/third_party/FreeRTOS-Kernel)
set(FREERTOS_CONFIG_FILE_DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/firmware/apps)
add_subdirectory(
    third_party/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040
    FreeRTOS-Kernel)

add_subdirectory(common)
add_subdirectory(third_party/waveshare/Config)
add_subdirectory(third_party/waveshare/Fonts)
add_subdirectory(third_party/waveshare/GUI)
add_subdirectory(third_party/waveshare/LCD)
add_subdirectory(platform)
add_subdirectory(firmware/apps/system)
add_subdirectory(ui)
add_subdirectory(apps/launcher)
add_subdirectory(apps/snake)
add_subdirectory(apps/temperature)
add_subdirectory(apps/settings)
add_subdirectory(apps/weather)
add_subdirectory(firmware/apps)
