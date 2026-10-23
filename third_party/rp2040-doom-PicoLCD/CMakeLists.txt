cmake_minimum_required(VERSION 3.13)

# We use PICO_SDK_PATH being set as an indicator that we're doing a Pico BUILD
if (PICO_SDK_PATH)
    include(pico_sdk_import.cmake)
    include(pico_extras_import.cmake)
endif()

project("Chocolate Doom" VERSION 3.0.0 LANGUAGES C CXX)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 14)
option(DOOM_ENABLE_AUDIO "Build RP2040 I2S sound and music support" OFF)
option(DOOM_BUILD_WHD_GEN "Build the host-side WAD to WHD/WHX converter" OFF)

if (PICO_SDK_PATH)
    # USB targets use the port's optional TinyUSB submodule when it is present.
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/3rdparty/tinyusb/CMakeLists.txt")
        set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/3rdparty/tinyusb)
    endif()
    # this only affects device builds device, but we want to use zone for malloc in this case so we don't have two separate elastic spaces and can fit more in
    set(SKIP_PICO_MALLOC 1)
    pico_sdk_init()
    if (PICO_ON_DEVICE AND NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        message(WARNING "You should do a MinSizeRel build when targeting the RP2040
        (with -DCMAKE_BUILD_TYPE=MinSizeRel)")
    endif()
endif()

set(CMAKE_C_STANDARD 11)

# Autotools variables
set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})

# AC_INIT variables
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "chocolate-doom")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "chocolate-doom-dev-list@chocolate-doom.org")

string(REGEX REPLACE " Doom$" "" PACKAGE_SHORTNAME "${PACKAGE_NAME}")
set(PACKAGE_COPYRIGHT "Copyright (C) 1993-2017")
set(PACKAGE_LICENSE "GNU General Public License, version 2")

# Any settings that should apply to all targets in this directory and all
# subdirectories should go here.  Use judiciously.
if(MSVC)
    add_definitions("/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_SECURE_NO_DEPRECATE"
                    "/D_CRT_NONSTDC_NO_DEPRECATE")
else()
    #add_compile_options("-Wall" "-Wdeclaration-after-statement" "-Wredundant-decls")
    add_compile_options("-Wall" "-Wredundant-decls")
endif()

# The retained port is device-only and does not use host filesystem features.
set(HAVE_MMAP 0)
set(HAVE_DIRENT_H 0)
set(HAVE_DECL_STRCASECMP 0)
set(HAVE_DECL_STRNCASECMP 0)

string(CONCAT WINDOWS_RC_VERSION "${PROJECT_VERSION_MAJOR}, "
    "${PROJECT_VERSION_MINOR}, ${PROJECT_VERSION_PATCH}, 0")

# Without a hyphen. This is used for the bash-completion scripts.
string(TOLOWER "${PACKAGE_SHORTNAME}" PROGRAM_SPREFIX)

# With a hyphen, used almost everywhere else.
set(PROGRAM_PREFIX "${PROGRAM_SPREFIX}-")

configure_file(cmake/config.h.cin config.h)

# This vendored copy is intentionally Pico-only. Desktop launchers, setup tools,
# sound backends and packaging were removed because the project only builds the
# RP2040 ST7789 firmware.
add_subdirectory(src)
