# Create a high-level runtime module that owns the Application logic and initialization
# This module depends on all other engine modules, but modules don't depend on it.
add_library(engine_app STATIC
    application.h
    application.cpp
    application_types.h
    entry_point.h
)

target_include_directories(engine_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_app PUBLIC
    engine_scripting
    engine_scene
    engine_project
    engine_graphics
    engine_physics
    engine_audio
    engine_assets
    engine_core
    engine_imgui
    engine_platform
    engine_network
)

target_include_directories(engine_app PRIVATE ${CMAKE_SOURCE_DIR})

if(TARGET engine_pch)
    target_link_libraries(engine_app PRIVATE engine_pch)
endif()
