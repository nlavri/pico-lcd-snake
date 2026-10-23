add_library(launcher_app STATIC
    launcher_app.c
)

target_include_directories(launcher_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(launcher_app
    PUBLIC
        app_framework
)
