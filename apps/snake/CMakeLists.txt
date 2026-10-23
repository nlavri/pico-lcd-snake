add_library(snake_app STATIC
    snake_app.c
    snake_game.c
)

target_include_directories(snake_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(snake_app
    PUBLIC
        app_framework
        app_ui
)
