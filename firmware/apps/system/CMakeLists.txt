add_library(app_framework STATIC
    app_manager.c
    input_events.c
)

target_include_directories(app_framework
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(app_framework
    PUBLIC
        app_platform
        project_logger
)

add_subdirectory(services)
