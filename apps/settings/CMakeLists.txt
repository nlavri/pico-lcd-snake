add_library(settings_app STATIC
    settings_app.c
)

target_include_directories(settings_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(settings_app
    PUBLIC
        app_framework
        app_platform
        app_ui
)
