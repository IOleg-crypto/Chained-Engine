# Unit & Integration tests for Editor modules
# Links against editor_core, engine modules, and GoogleTest.

set(EDITOR_TEST_SOURCES
    test_main_editor.cpp
    command_history_tests.cpp
    editor_camera_tests.cpp
    asset_dependency_tests.cpp
)

add_executable(engine_tests_editor ${EDITOR_TEST_SOURCES})

target_link_libraries(engine_tests_editor PRIVATE
    editor_core
    gtest
    gtest_main
)

# MinGW/gcc: statically link the gcc/stdc++ runtime so the test exe can start
# during test discovery without libstdc++-6.dll / libgcc_s / libwinpthread on PATH.
if(MINGW)
    target_link_options(engine_tests_editor PRIVATE -static-libgcc -static-libstdc++)
endif()

target_include_directories(engine_tests_editor PRIVATE
    ${CMAKE_SOURCE_DIR}
)

if(TARGET engine_pch)
    target_link_libraries(engine_tests_editor PRIVATE engine_pch)
endif()

include(GoogleTest)
# Discover individual GTest tests so each shows up as a separate CTest entry.
gtest_discover_tests(engine_tests_editor
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    PROPERTIES
        LABELS "Editor"
        ENVIRONMENT "ENGINE_ROOT=${CMAKE_SOURCE_DIR}"
        TIMEOUT 60
)
