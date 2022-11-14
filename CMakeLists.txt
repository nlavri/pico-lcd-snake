# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "c:/Projects/rpi_pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-lcd-snake C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add a compilation subdirectory
add_subdirectory(lib/Config)
add_subdirectory(lib/Fonts)
add_subdirectory(lib/GUI)
add_subdirectory(lib/LCD)


# Add a header directory
include_directories(./lib/Config)
include_directories(./lib/Fonts)
include_directories(./lib/GUI)
include_directories(./lib/LCD)


# Add executable. Default name is the project name, version 0.1

add_executable(pico-lcd-snake main.c runtime.c output_hal.c input_hal.c game.c )

pico_set_program_name(pico-lcd-snake "pico-lcd-snake")
pico_set_program_version(pico-lcd-snake "0.1")

pico_enable_stdio_uart(pico-lcd-snake 0)
pico_enable_stdio_usb(pico-lcd-snake 1)

# Add the standard library to the build
target_link_libraries(pico-lcd-snake pico_stdlib LCD GUI Fonts Config hardware_spi)

pico_add_extra_outputs(pico-lcd-snake)

