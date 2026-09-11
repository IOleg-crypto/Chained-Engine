# Define the Game module as a Standalone Executable
# Signature: TARGET_NAME PROJECT_GAME [CSHARP_PROJECT_PATH] [native_sources...]

# Embed Windows application icon and UAC manifest into the .exe binary
set(GAME_RC_FILE "")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ChainedDecos.rc")
    set(GAME_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/ChainedDecos.rc")
endif()

chained_add_game(ChainedDecos
    PROJECT_GAME ChainedDecos
    CSHARP_PROJECT "assets/scripts/ChainedDecos.Scripts.csproj"
    RC_SOURCES ${GAME_RC_FILE}
)

# Copy game assets (only if source files changed)
# Skipped when CH_SOURCE_ASSETS_DIR is set (dev mode — runtime reads from source tree)
set(GAME_ASSET_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(GAME_ASSET_DST_DIR "$<TARGET_FILE_DIR:ChainedDecosExe>/assets")

if(NOT CH_SOURCE_ASSETS_DIR OR NOT EXISTS "${CH_SOURCE_ASSETS_DIR}")
    add_custom_command(TARGET ChainedDecosExe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_ASSET_DST_DIR}"
        COMMAND ${CMAKE_COMMAND} -DSOURCE="${GAME_ASSET_SRC_DIR}" -DDEST="${GAME_ASSET_DST_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfDifferent.cmake"
        COMMENT "Syncing game assets from ${GAME_ASSET_SRC_DIR} to ${GAME_ASSET_DST_DIR}..."
    )
else()
    message(STATUS "Dev mode: ChainedDecos reads game assets from ${CH_SOURCE_ASSETS_DIR}")
endif()

# Copy project file next to the executable so RuntimeLayer can find it
add_custom_command(TARGET ChainedDecosExe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ChainedDecos.chproject"
        "$<TARGET_FILE_DIR:ChainedDecosExe>/ChainedDecos.chproject"
    COMMENT "Copying ChainedDecos.chproject to output directory..."
)

# Sync engine resources (shaders, fonts, icons, config) to game output
if(COMMAND ch_add_resource_sync)
    ch_add_resource_sync(ChainedDecosExe)
endif()

# Pass the absolute icon path to the Windows resource compiler
if(WIN32 AND GAME_RC_FILE)
    set(ICON_ABS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/chaineddecos.ico")
    set_source_files_properties("${GAME_RC_FILE}" PROPERTIES
        COMPILE_DEFINITIONS "CHAINED_ICON_PATH=\"${ICON_ABS_PATH}\""
    )
endif()
