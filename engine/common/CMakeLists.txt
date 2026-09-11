set(COMMON_SOURCES
    base.h
    color.h
    engine_assert.h
    platform_detection.h
    thread_pool.h
    timestep.h
    uuid.h
    uuid.cpp
)

add_library(engine_common STATIC
    ${COMMON_SOURCES}
)

target_include_directories(engine_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_common PUBLIC
    spdlog
    glm
    libzstd_static
)
