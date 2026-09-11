set(SCRIPTING_ENGINE
    scriptengine.h
    scriptengine.cpp
    scriptengine_services.h
    scriptengine_services.cpp
    scene_scripting_manager.h
    scene_scripting_manager.cpp
)

set(SCRIPTING_GLUE
    script_glue.h
    script_glue.cpp
    script_glue_internal.h
    script_glue_internal.cpp
    script_glue_system.h
    script_glue_system.cpp
    script_glue_input.h
    script_glue_input.cpp
    script_glue_scene.h
    script_glue_scene.cpp
    script_glue_entity.h
    script_glue_entity.cpp
    script_glue_transform.cpp
    script_glue_physics.cpp
    script_glue_animation.cpp
    script_glue_camera.h
    script_glue_camera.cpp
    script_glue_ui.h
    script_glue_ui.cpp
    script_glue_audio.h
    script_glue_audio.cpp
    script_glue_network.h
    script_glue_network.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/generated/script_glue_generated.h
    ${CMAKE_CURRENT_SOURCE_DIR}/generated/script_glue_generated.cpp
)

add_library(engine_scripting STATIC
    ${SCRIPTING_ENGINE}
    ${SCRIPTING_GLUE}
)

target_include_directories(engine_scripting PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_scripting PUBLIC
    engine_core
    engine_audio
    engine_physics
    engine_imgui
    engine_graphics
    engine_network
    Coral.Native
)

if(TARGET engine_pch)
    target_link_libraries(engine_scripting PRIVATE engine_pch)
endif()

target_include_directories(engine_scripting PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/coral/Coral.Native/Include
    ${CMAKE_SOURCE_DIR}/thirdparty/coral/NetCore
)


if(MINGW)
    # See cmake/external/coral.cmake: MinGW -O0 codegen can misalign the stack
    # on the call path into CoreCLR, causing Debug-only access violations on
    # CI. -Og overrides the global Debug -O0 for this bridge module only.
    target_compile_options(engine_scripting PRIVATE $<$<CONFIG:Debug>:-Og>)
endif()


# ── Generate ManagedDependencies.props for C# build ───────────────────────────
# This tells MSBuild where to find Coral.Managed.dll. The path changes per
# build preset (windows-clang, windows-gcc, etc.), so we generate it from a
# template at configure time using the correct CMAKE_BINARY_DIR.
# On Linux / WSL2 the source tree may live on an NTFS mount (/mnt/d/) which
# does not support configure_file writes into the source directory. The .csproj
# imports the file conditionally (Condition="Exists(...)"), so skipping it on
# non-Windows is safe — dotnet build CLI does not rely on it.
set(CORAL_MANAGED_DIR "${CMAKE_BINARY_DIR}/vendor/coral")
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/managed/ManagedDependencies.props.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/managed/ManagedDependencies.props"
        @ONLY
    )
endif()

set(MANAGED_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/managed")
set(MANAGED_PROJECT_PATH "${MANAGED_PROJECT_DIR}/Chained.Managed.csproj")

set(MANAGED_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CORAL_MANAGED_DIR "${CMAKE_BINARY_DIR}/vendor/coral")

set(BUILD_MANAGED_SCRIPT "${CMAKE_SOURCE_DIR}/tools/build_managed.py")

file(GLOB_RECURSE MANAGED_SOURCES "${MANAGED_PROJECT_DIR}/src/*.cs")

set(MANAGED_DLL_OUTPUT "${MANAGED_OUTPUT_DIR}/Chained.Managed.dll")

add_custom_command(
    OUTPUT  "${MANAGED_DLL_OUTPUT}"
    COMMAND Python3::Interpreter "${BUILD_MANAGED_SCRIPT}"
            --project "${MANAGED_PROJECT_PATH}"
            --output "${MANAGED_OUTPUT_DIR}"
            --coral-dir "${CORAL_MANAGED_DIR}"
            --configuration $<CONFIG>
            --copy-coral   
            --write-props  
            --parallel     
    WORKING_DIRECTORY "${MANAGED_PROJECT_DIR}"
    DEPENDS ${MANAGED_SOURCES} "${MANAGED_PROJECT_PATH}"
    COMMENT "Building Chained.Managed C# Library"
    VERBATIM
)

# 3. Створення таргету та зв'язування залежностей
add_custom_target(Chained_Managed ALL DEPENDS "${MANAGED_DLL_OUTPUT}")

if(TARGET Coral.Native)
    add_dependencies(Chained_Managed Coral.Native)
endif()

add_dependencies(engine_scripting Chained_Managed)

# ── Auto-generate C++ glue from C# component wrappers ────────────────────────
set(GLUE_GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/tools/generate_glue.py")
set(GLUE_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(GLUE_OUTPUTS
    "${GLUE_OUTPUT_DIR}/script_glue_generated.h"
    "${GLUE_OUTPUT_DIR}/script_glue_generated.cpp"
    "${GLUE_OUTPUT_DIR}/script_glue_generated_reg.inl"
)

add_custom_command(
    OUTPUT  ${GLUE_OUTPUTS}
    COMMAND Python3::Interpreter "${GLUE_GENERATOR_SCRIPT}"
            --cs-dir "${MANAGED_PROJECT_DIR}/src/Components"
            --output-dir "${GLUE_OUTPUT_DIR}"
            --classes PlayerComponent SpawnComponent NetworkIdentityComponent
    DEPENDS ${GLUE_GENERATOR_SCRIPT}
            ${MANAGED_SOURCES}
    COMMENT "Generating C++ glue from C# [NativeProperty]/[NativeCall] attributes"
    VERBATIM
)

add_custom_target(GlueGenerated DEPENDS ${GLUE_OUTPUTS})
add_dependencies(engine_scripting GlueGenerated)