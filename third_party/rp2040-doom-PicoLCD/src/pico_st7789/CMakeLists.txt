add_library(pico_st7789 INTERFACE)

target_sources(pico_st7789 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/st7789.c
)

target_include_directories(pico_st7789 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/include
)

target_link_libraries(pico_st7789 INTERFACE pico_stdlib hardware_spi)
