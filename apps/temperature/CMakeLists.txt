add_library(temperature_app STATIC
    temperature_app.c
)

target_include_directories(temperature_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(temperature_app
    PUBLIC
        app_framework
)
