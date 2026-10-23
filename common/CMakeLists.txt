add_library(project_logger STATIC
    logger.c
)

target_include_directories(project_logger
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)
