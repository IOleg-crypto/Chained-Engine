# Integration tests — require engine runtime (Application, Scene, Physics, Renderer, Scripting)
set(INTEGRATION_SOURCES
    scene_tests.cpp
    scene_serialization_tests.cpp
    async_scene_loader_tests.cpp
    physics_tests.cpp
    scriptengine_tests.cpp
    renderer_tests.cpp
    audio_tests.cpp
    importer_tests.cpp
    integration_tests.cpp
    asset_manager_tests.cpp
    material_tests.cpp
    hierarchy_tests.cpp
    serialization_tests.cpp
    network_tests.cpp
    smoke_tests.cpp
    regression_tests.cpp
    test_environment.cpp
    test_main.cpp
)

add_executable(engine_tests_integration ${INTEGRATION_SOURCES})

target_link_libraries(engine_tests_integration PRIVATE
    engine_core
    engine_app
    engine_scene
    engine_graphics
    engine_physics
    engine_audio
    engine_scripting
    engine_project
    engine_assets
    engine_network
    engine_external_imgui
    engine_external_entt
    engine_reflection
    gtest
    gtest_main
)

# MinGW/gcc: statically link the gcc/stdc++ runtime so the test exe can start
# during test discovery without libstdc++-6.dll / libgcc_s / libwinpthread on PATH.
if(MINGW)
    target_link_options(engine_tests_integration PRIVATE -static-libgcc -static-libstdc++)
endif()

target_include_directories(engine_tests_integration PRIVATE
    ${CMAKE_SOURCE_DIR}
)

if(TARGET engine_pch)
    target_link_libraries(engine_tests_integration PRIVATE engine_pch)
endif()

include(GoogleTest)
# Discover individual GTest tests so each shows up as a separate CTest entry.
# POST_BUILD mode discovers tests at build time, avoiding PRE_TEST path issues
# on Windows and the overhead of running the discovery binary at test time.
gtest_discover_tests(engine_tests_integration
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        LABELS "Integration"
        ENVIRONMENT "ENGINE_ROOT=${CMAKE_SOURCE_DIR}"
        TIMEOUT 120
)