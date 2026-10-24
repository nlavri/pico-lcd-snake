add_library(ssh_app STATIC
    ssh_app.c
)

target_include_directories(ssh_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ssh_app
    PUBLIC
        app_framework
        app_platform
        app_ui
)
