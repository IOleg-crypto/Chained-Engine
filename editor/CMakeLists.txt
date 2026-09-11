
set(EDITOR_SOURCES
    action_commands.h
    action_commands.cpp
    asset_types.h
    editor_colors.h
    editor_menu.h
    editor_menu.cpp
    events.h
    events.cpp
    font_choice_gui.h
    font_choice_gui.cpp
    font_manager.h
    font_manager.cpp
    gui.h
    gui.cpp
    icons.h
    layer.h
    layer.cpp
    layout.h
    layout.cpp
    panels.h
    panels.cpp
    project_manager.h
    project_manager.cpp
    scene_manager.h
    scene_manager.cpp
    scene_picking.h
    scene_picking.cpp
    types.h
    ui_properties.h
    ui_properties.cpp

    panels/panel.h
    panels/anim_graph_panel.h
    panels/anim_graph_panel.cpp
    panels/console_panel.h
    panels/console_panel.cpp
    panels/content_browser_panel.h
    panels/content_browser_panel.cpp
    panels/effects_panel.h
    panels/effects_panel.cpp
    panels/inspector_panel.h
    panels/inspector_panel.cpp
    panels/material_panel.h
    panels/material_panel.cpp
    panels/profiler_panel.h
    panels/profiler_panel.cpp
    panels/project_settings_panel.h
    panels/project_settings_panel.cpp
    panels/property_editor.h
    panels/property_editor.cpp
    panels/scene_hierarchy_panel.h
    panels/scene_hierarchy_panel.cpp
    panels/viewport_panel.h
    panels/viewport_panel.cpp
    panels/world_panel.h
    panels/world_panel.cpp

    project/asset_dependency_collector.h
    project/asset_dependency_collector.cpp
    project/binary_deployer.h
    project/binary_deployer.cpp
    project/editor_settings.h
    project/project_exporter.h
    project/project_exporter.cpp
    project/project_serializer.h
    project/project_serializer.cpp
    project/resource_packer.h
    project/resource_packer.cpp
    project/texture_compressor.h
    project/texture_compressor.cpp

    ui/project_selector_ui.h
    ui/project_selector_ui.cpp

    undo/command.h
    undo/command_history.h
    undo/command_history.cpp
    undo/component_commands.h
    undo/entity_commands.h
    undo/modify_component_command.h

    viewport/camera.h
    viewport/camera.cpp
    viewport/gizmo.h
    viewport/gizmo.cpp
    viewport/ui_manipulator.h
    viewport/ui_manipulator.cpp
    viewport/viewport_icons.h
    viewport/viewport_icons.cpp
    viewport/viewport_picking.h
    viewport/viewport_picking.cpp
    viewport/viewport_renderer.h
    viewport/viewport_renderer.cpp
    viewport/viewport_toolbar.h
    viewport/viewport_toolbar.cpp
)

if(CH_NETWORKING)
    list(APPEND EDITOR_SOURCES
        panels/network_panel.h
        panels/network_panel.cpp
    )
endif()

add_library(editor_core STATIC ${EDITOR_SOURCES})

target_link_libraries(editor_core PUBLIC
    ChainedEngine::Framework
)

# Pack library — asset packing for shipped builds
if(TARGET pack-static)
    target_link_libraries(editor_core PUBLIC pack-static)
endif()

if(TARGET engine_external_basisu_encoder)
    target_link_libraries(editor_core PUBLIC engine_external_basisu_encoder)
endif()

# WinHTTP — used by NetworkPanel to fetch the public IP from api.ipify.org.
# Only needed when the networking module is enabled.
if(WIN32 AND CH_NETWORKING)
    target_link_libraries(editor_core PUBLIC winhttp)
endif()

target_include_directories(editor_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Define the Editor executable
add_executable(ChainedEditor ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# Embed Windows application icon into the editor executable
set(EDITOR_RC_FILE "")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ChainedEditor.rc")
    set(EDITOR_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ChainedEditor.rc")
    target_sources(ChainedEditor PRIVATE ${EDITOR_RC_FILE})
    set(EDITOR_ICON_ABS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/icons/chaineddecos.ico")
    set_source_files_properties("${EDITOR_RC_FILE}" PROPERTIES
        COMPILE_DEFINITIONS "CHAINED_ICON_PATH=\"${EDITOR_ICON_ABS_PATH}\""
    )
endif()

target_link_libraries(ChainedEditor PRIVATE editor_core)

# Packer utility — build-time tool, not linked to the editor
if(TARGET packer)
    add_dependencies(ChainedEditor packer)
endif()

if(TARGET engine_pch)
    target_link_libraries(editor_core PRIVATE engine_pch)
    target_link_libraries(ChainedEditor PRIVATE engine_pch)
endif()

# Automatically sync resources/ into the correct bin/<Config>/ directory after each build
if(COMMAND ch_add_resource_sync)
    ch_add_resource_sync(ChainedEditor)
endif()

# Installation
install(TARGETS ChainedEditor
    RUNTIME DESTINATION bin COMPONENT Editor
)

# Copy global assets to the install directory if they exist
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION assets COMPONENT Runtime)
endif()
