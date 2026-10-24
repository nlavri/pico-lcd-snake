add_library(calculator_app STATIC
    calculator_app.c
    calculator_engine.c
)

target_include_directories(calculator_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(calculator_app
    PUBLIC
        app_framework
        app_ui
)
