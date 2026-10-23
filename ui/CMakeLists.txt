add_library(app_ui STATIC
    ui.c
)

target_include_directories(app_ui
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(app_ui
    PUBLIC
        app_framework
        app_platform
)
