# Unit tests — fast, no full engine runtime required
# Links against specific engine modules, not the full engine facade.

set(UNIT_SOURCES
    test_main_unit.cpp
    core_types_tests.cpp
    event_system_tests.cpp
    thread_pool_tests.cpp
    layer_stack_tests.cpp
    service_locator_tests.cpp
    byte_codec_tests.cpp
    network_interpolation_tests.cpp
    transform_tests.cpp
    project_tests.cpp
    ${CMAKE_SOURCE_DIR}/engine/scene/systems/transform_system.cpp
)

add_executable(engine_tests_unit ${UNIT_SOURCES})

target_link_libraries(engine_tests_unit PRIVATE
    engine_common
    engine_core
    engine_project
    engine_assets
    engine_audio
    engine_network
    engine_external_entt
    glm
    gtest
    gtest_main
)

# MinGW/gcc: statically link the gcc/stdc++ runtime so the test exe can start
# during test discovery without libstdc++-6.dll / libgcc_s / libwinpthread on PATH.
if(MINGW)
    target_link_options(engine_tests_unit PRIVATE -static-libgcc -static-libstdc++)
endif()

target_include_directories(engine_tests_unit PRIVATE
    ${CMAKE_SOURCE_DIR}
)

if(TARGET engine_pch)
    target_link_libraries(engine_tests_unit PRIVATE engine_pch)
endif()

include(GoogleTest)
# Discover individual GTest tests so each shows up as a separate CTest entry.
# Unit tests are fast (no engine boot), so per-test discovery has negligible overhead.
gtest_discover_tests(engine_tests_unit
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    PROPERTIES
        LABELS "Unit"
        ENVIRONMENT "ENGINE_ROOT=${CMAKE_SOURCE_DIR}"
        TIMEOUT 30
)
