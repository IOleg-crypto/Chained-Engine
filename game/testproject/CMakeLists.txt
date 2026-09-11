# Define the Game module as a Standalone Executable
# Signature: TARGET_NAME PROJECT_GAME [CSHARP_PROJECT_PATH] [native_sources...]
chained_add_game(TestProjectGame
    PROJECT_GAME testproject
)

# Copy engine resources so the standalone executable behaves like the main game.
# Skipped when CH_SOURCE_RESOURCES_DIR is set (dev mode — runtime reads from source tree)
if(NOT CH_SOURCE_RESOURCES_DIR OR NOT EXISTS "${CH_SOURCE_RESOURCES_DIR}")
    add_custom_command(TARGET TestProjectGameExe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:TestProjectGameExe>/resources"
        COMMENT "Copying engine resources..."
    )
endif()

# Copy test project assets into the output folder.
# Skipped when CH_SOURCE_ASSETS_DIR is set (dev mode — runtime reads from source tree)
if(NOT CH_SOURCE_ASSETS_DIR OR NOT EXISTS "${CH_SOURCE_ASSETS_DIR}")
    add_custom_command(TARGET TestProjectGameExe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:TestProjectGameExe>/assets"
        COMMENT "Copying test project assets..."
    )
endif()
