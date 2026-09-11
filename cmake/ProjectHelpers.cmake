# Helper function to compile C# scripts for a game project
# Usage: chained_add_csharp_scripts(TARGET_NAME CSHARP_PROJECT_PATH)
function(chained_add_csharp_scripts TARGET_NAME CSHARP_PROJECT_PATH)
    set(SCRIPT_TARGET "BuildScripts_${TARGET_NAME}")
    if(TARGET ${SCRIPT_TARGET})
        return()
    endif()

    set(FULL_CSPROJ_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${CSHARP_PROJECT_PATH}")
    if(NOT EXISTS "${FULL_CSPROJ_PATH}")
        message(WARNING "chained_add_csharp_scripts: Could not find ${FULL_CSPROJ_PATH}")
        return()
    endif()

    set(CORAL_MANAGED_DIR "${CMAKE_BINARY_DIR}/vendor/coral")
    set(SCRIPT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/scripts/${TARGET_NAME}")
    set(SCRIPT_DLL_PATH "${SCRIPT_OUTPUT_DIR}/${TARGET_NAME}.dll")

    add_custom_target(${SCRIPT_TARGET}
        COMMAND "${CH_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_managed.py"
            --project "${FULL_CSPROJ_PATH}"
            --configuration $<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:>>,Debug,Release>
            --output "${SCRIPT_OUTPUT_DIR}"
            --coral-dir "${CORAL_MANAGED_DIR}"
            --parallel
        COMMAND "${CH_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/sync_scripts.py"
            --build-dir "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
            --game-dir "${CMAKE_CURRENT_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building C# Scripts for ${TARGET_NAME} (incremental)"
        BYPRODUCTS "${SCRIPT_DLL_PATH}"
        VERBATIM
    )
    
    # Ensure scripts build AFTER Chained_Managed to avoid dotnet race condition
    if(TARGET Chained_Managed)
        add_dependencies(${SCRIPT_TARGET} Chained_Managed)
    endif()
endfunction()

# Internal helper to apply common configuration to game-related targets
macro(_chained_configure_game_target TGT)
    target_include_directories(${TGT} PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    if(TARGET engine_pch)
        target_link_libraries(${TGT} PRIVATE engine_pch)
    endif()
endmacro()

# Main helper function to create a standalone game project
# Usage: 
# chained_add_game(TARGET_NAME
#    DISPLAY_NAME "Project Title"
#    CSHARP_PROJECT "src/Scripts.csproj" # Optional
#    SOURCES src/file1.cpp src/file2.cpp # Optional
# )
function(chained_add_game TARGET_NAME)
    set(options)
    set(oneValueArgs PROJECT_GAME CSHARP_PROJECT)
    set(multiValueArgs SOURCES RC_SOURCES)
    cmake_parse_arguments(GAME "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 1. Locate the entry point (main.cpp) (OPTIONAL)
    set(ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    set(HAS_ENTRY_POINT OFF)
    if(EXISTS "${ENTRY_SOURCE}")
        set(HAS_ENTRY_POINT ON)
    endif()

    # 2. Compile game sources as a static library (saves RAM and compile time)
    if(GAME_SOURCES)
        add_library(${TARGET_NAME} STATIC ${GAME_SOURCES})
        # Rename static lib so its .lib doesn't clash with the IMPLIB generated
        # by the exe on MSVC (both would be ChainedDecos.lib otherwise → LNK1181).
        set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${TARGET_NAME}Game")
        target_link_libraries(${TARGET_NAME} PUBLIC ChainedEngine::Framework)
        _chained_configure_game_target(${TARGET_NAME})
    endif()

    # 3. Create the EXECUTABLE target
    if(HAS_ENTRY_POINT)
        # RC_SOURCES (e.g. app icon .rc) go directly into the exe, not the static lib
        add_executable(${TARGET_NAME}Exe ${ENTRY_SOURCE} ${GAME_RC_SOURCES})
        target_link_libraries(${TARGET_NAME}Exe PRIVATE engine_runtime_core)

        if(GAME_SOURCES)
             target_link_libraries(${TARGET_NAME}Exe PRIVATE ${TARGET_NAME})
        endif()

        set_target_properties(${TARGET_NAME}Exe PROPERTIES
            OUTPUT_NAME "${TARGET_NAME}"
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        target_compile_definitions(${TARGET_NAME}Exe PRIVATE
            GAME_BUILD_EXE
        )
        _chained_configure_game_target(${TARGET_NAME}Exe)
    endif()

    # 4. Handle C# Script Building
    if(GAME_CSHARP_PROJECT)
        chained_add_csharp_scripts(${TARGET_NAME} "${GAME_CSHARP_PROJECT}")
        if(HAS_ENTRY_POINT)
            add_dependencies(${TARGET_NAME}Exe "BuildScripts_${TARGET_NAME}")
        endif()
    endif()

    # 5. Installation
    if(HAS_ENTRY_POINT)
        set(INSTALL_TARGETS ${TARGET_NAME}Exe)
        if(TARGET ${TARGET_NAME})
            list(APPEND INSTALL_TARGETS ${TARGET_NAME})
        endif()

        install(TARGETS ${INSTALL_TARGETS}
            RUNTIME DESTINATION bin COMPONENT Game
            ARCHIVE DESTINATION lib COMPONENT SDK
        )
    endif()

    message(STATUS "Configured Project: ${GAME_PROJECT_GAME} (Output=${TARGET_NAME})")
endfunction()

# Copy engine resources (shaders, fonts, icons, config) to a target's output directory
# Skipped when CH_SOURCE_RESOURCES_DIR is set (dev mode — runtime reads from source tree)
function(ch_add_resource_sync TARGET)
    if(CH_SOURCE_RESOURCES_DIR AND EXISTS "${CH_SOURCE_RESOURCES_DIR}")
        message(STATUS "Dev mode: ${TARGET} reads engine resources from ${CH_SOURCE_RESOURCES_DIR}")
        return()
    endif()

    set(RESOURCES_SRC "${CMAKE_SOURCE_DIR}/resources")
    set(RESOURCES_DST "$<TARGET_FILE_DIR:${TARGET}>/resources")

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCES_DST}"
        COMMAND ${CMAKE_COMMAND} -DSOURCE="${RESOURCES_SRC}" -DDEST="${RESOURCES_DST}" -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfDifferent.cmake"
        COMMENT "Syncing engine resources to ${RESOURCES_DST}..."
    )
endfunction()


