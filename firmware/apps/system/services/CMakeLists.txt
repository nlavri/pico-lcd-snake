add_library(network_service STATIC
    network_service.c
)

target_include_directories(network_service
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/firmware/apps
)

target_link_libraries(network_service
    PUBLIC
        app_platform
    PRIVATE
        FreeRTOS-Kernel-Heap4
        project_logger
)

if(PICO_CYW43_SUPPORTED)
    target_compile_definitions(network_service PRIVATE APP_NETWORK_PICO_W=1)
    target_link_libraries(network_service
        PRIVATE
            pico_cyw43_arch_lwip_sys_freertos
    )
endif()
