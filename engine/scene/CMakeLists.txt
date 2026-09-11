# ── Source groups ─────────────────────────────────────────────────────────────

set(SCENE_CORE_SOURCES
    entity.cpp entity.h
    scene.h
    scene_events.h scene_settings.h scene_state.h
    camera.cpp camera.h
    components.h
)

set(SCENE_COMPONENTS
    components/core/transform_component.h
    components/core/id_component.h
    components/core/tag_component.h
    components/core/hierarchy_component.h
    components/render/camera_component.h
    components/render/light_component.h
    components/render/model_component.h
    components/render/sprite_component.h
    components/render/shader_component.h
    components/physics/physics_component.h
    components/audio/audio_component.h
    components/animation/animation_component.h
    components/animation/animation_graph_component.h
    components/ui/control_component.h
    components/ui/ui_action_component.h
    components/ui/scene_transition_component.h
    components/gameplay/player_component.h
    components/gameplay/spawn_component.h
    components/gameplay/network_identity_component.h
    components/scripting/scripting_components.h
    components/scripting/scripting_components.cpp
)

set(SCENE_REGISTRY
    component_registry.cpp
    component_registry_core.cpp
    component_registry_gameplay.cpp
    component_registry_physics.cpp
    component_registry_rendering.cpp
    component_registry_scripting.cpp
    component_registry_ui.cpp
    component_ui_control.cpp
)

set(SCENE_SERIALIZERS
    component_serializer.cpp
    hierarchy_serializer.cpp
    prefab_serializer.cpp
    scene.cpp
    scene_serializer.cpp
    async_scene_loader.h
    async_scene_loader.cpp
)

set(SCENE_SYSTEMS
    systems/hierarchy_system.h
    systems/hierarchy_system.cpp
    systems/transform_system.h
    systems/transform_system.cpp
    systems/animation_system.h
    systems/animation_system.cpp
    systems/audio_system.h
    systems/audio_system.cpp
    systems/physics_body_system.h
    systems/physics_body_system.cpp
    systems/asset_resolution_system.h
    systems/asset_resolution_system.cpp
    systems/scene_transition_system.h
    systems/scene_transition_system.cpp
    systems/network_system.h
    systems/network_system.cpp
    systems/camera_auto_select_system.h
    systems/camera_auto_select_system.cpp
)

# ── 1. Core library (No UI / Graphics dependencies) ───────────────────────────
add_library(engine_scene_core STATIC
    ${SCENE_CORE_SOURCES}
    ${SCENE_COMPONENTS}
)
target_include_directories(engine_scene_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_link_libraries(engine_scene_core PUBLIC engine_reflection engine_assets engine_external_entt glm)
if(TARGET engine_pch)
    target_link_libraries(engine_scene_core PRIVATE engine_pch)
endif()
set_target_properties(engine_scene_core PROPERTIES UNITY_BUILD OFF)

# ── 2. Main scene library (Serialization, Registry, UI Components) ────────────
add_library(engine_scene STATIC
    ${SCENE_REGISTRY}
    ${SCENE_SERIALIZERS}
    ${SCENE_SYSTEMS}
)

target_include_directories(engine_scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_scene PUBLIC
    engine_scene_core
    engine_reflection
    engine_core
    engine_graphics
    engine_ui
    engine_project
    engine_physics
    engine_audio
    engine_scripting
    engine_network
    engine_external_entt
    glm
    engine_external_imgui
)

if(TARGET engine_pch)
    target_link_libraries(engine_scene PRIVATE engine_pch)
endif()

set_target_properties(engine_scene PROPERTIES UNITY_BUILD OFF)
