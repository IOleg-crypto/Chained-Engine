set(UI_SYSTEMS
    ui_animation_system.h
    ui_animation_system.cpp
    ui_control_renderer.h
    ui_control_renderer.cpp
    ui_data_components.h
    ui_data_components.cpp
    ui_font_registry.h
    ui_font_registry.cpp
    ui_input_system.h
    ui_input_system.cpp
    ui_layout_system.h
    ui_layout_system.cpp
    ui_style.h
    ui_types.h
    widget_renderer.h
    widget_renderer.cpp
)

set(UI_RENDER
    ui_render_basic.cpp
    ui_render_container.cpp
    ui_render_helpers.h
    ui_render_image.cpp
    ui_render_input.cpp
    ui_render_plot.cpp
    ui_render_widgets.h
)

add_library(engine_ui STATIC
    ${UI_SYSTEMS}
    ${UI_RENDER}
)

target_include_directories(engine_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_ui PUBLIC
    engine_scene_core
    engine_core
    engine_assets
    engine_project
    yaml-cpp
    engine_external_entt
    glm
    engine_external_imgui
)

if(TARGET engine_pch)
    target_link_libraries(engine_ui PRIVATE engine_pch)
endif()

set_target_properties(engine_ui PROPERTIES UNITY_BUILD OFF)
