add_library(platformer_demo_app STATIC
    platformer_demo_app.c
)

target_include_directories(platformer_demo_app
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(platformer_demo_app
    PUBLIC
        app_framework
)
