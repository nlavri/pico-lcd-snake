add_library(weather_app STATIC
    weather_app.c
)

target_include_directories(weather_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(weather_app
    PUBLIC
        app_framework
        app_platform
        app_ui
)
