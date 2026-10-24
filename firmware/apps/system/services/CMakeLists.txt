add_library(network_service STATIC
    network_service.c
)

add_library(ssh_service STATIC
    ssh_service.c
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

target_include_directories(ssh_service
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/firmware/apps
        ${CMAKE_SOURCE_DIR}/third_party/libssh2/pico
        ${PICO_SDK_PATH}/lib/mbedtls/include
        ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/include
)

target_link_libraries(ssh_service
    PUBLIC
        app_platform
    PRIVATE
        FreeRTOS-Kernel-Heap4
        project_logger
)

if(PICO_CYW43_SUPPORTED)
    target_compile_definitions(ssh_service PRIVATE
        APP_SSH_PICO_W=1
        MBEDTLS_CONFIG_FILE="pico_mbedtls_config.h")
    target_link_libraries(ssh_service PRIVATE
        libssh2_pico
        pico_cyw43_arch_lwip_sys_freertos
    )
endif()
