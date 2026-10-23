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

include(cmake/pico_sdk_import.cmake)

project(pico-lcd C CXX ASM)

pico_sdk_init()

add_subdirectory(common)
add_subdirectory(third_party/waveshare/Config)
add_subdirectory(third_party/waveshare/Fonts)
add_subdirectory(third_party/waveshare/GUI)
add_subdirectory(third_party/waveshare/LCD)
add_subdirectory(platform)
add_subdirectory(firmware/apps/system)
add_subdirectory(apps/launcher)
add_subdirectory(apps/snake)
add_subdirectory(apps/temperature)
add_subdirectory(firmware/apps)
