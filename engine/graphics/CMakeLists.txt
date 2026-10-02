set(GRAPHICS_API
    api/buffer.h
    api/buffer.cpp
    api/framebuffer.h
    api/framebuffer.cpp
    api/graphics_device.h
    api/graphics_device.cpp
    api/renderer_types.h
    api/shader.h
    api/shader.cpp
    api/storage_buffer.h
    api/storage_buffer.cpp
    api/texture.h
    api/texture.cpp
    api/vertex_array.h
    api/vertex_array.cpp
)

set(GRAPHICS_API_OPENGL
    api/opengl/gl_buffer.h
    api/opengl/gl_buffer.cpp
    api/opengl/gl_device.h
    api/opengl/gl_device.cpp
    api/opengl/gl_framebuffer.h
    api/opengl/gl_framebuffer.cpp
    api/opengl/gl_shader_program.h
    api/opengl/gl_shader_program.cpp
    api/opengl/gl_storage_buffer.h
    api/opengl/gl_storage_buffer.cpp
    api/opengl/gl_texture.h
    api/opengl/gl_texture.cpp
    api/opengl/gl_uniform_buffer.h
    api/opengl/gl_uniform_buffer.cpp
    api/opengl/gl_vertex_array.h
    api/opengl/gl_vertex_array.cpp
)

set(GRAPHICS_PIPELINE
    pipeline/debug_renderer.h
    pipeline/debug_renderer.cpp
    pipeline/frustum.h
    pipeline/geometry_generator.h
    pipeline/geometry_generator.cpp
    pipeline/material.h
    pipeline/renderer.h
    pipeline/renderer.cpp
    pipeline/renderer_data.h
    pipeline/lighting_manager.h
    pipeline/lighting_manager.cpp
    pipeline/geometry_factory.h
    pipeline/geometry_factory.cpp
    pipeline/entity_collector.h
    pipeline/entity_collector.cpp
    pipeline/material_manager.h
    pipeline/material_manager.cpp
    pipeline/render_pass.h
    pipeline/scene_renderer.h
    pipeline/scene_renderer.cpp
    pipeline/shader_storage.h
    pipeline/shader_storage.cpp
    pipeline/shader_uniform_utils.h
)

set(GRAPHICS_PASSES
    pipeline/passes/composite_pass.h
    pipeline/passes/composite_pass.cpp
    pipeline/passes/geometry_pass.h
    pipeline/passes/geometry_pass.cpp
    pipeline/passes/shadow_pass.h
    pipeline/passes/shadow_pass.cpp
    pipeline/passes/skybox_pass.h
    pipeline/passes/skybox_pass.cpp
)

set(GRAPHICS_MAIN
    camera_types.h
    text_renderer.h
    text_renderer.cpp
    freetype_gl_atlas.h
    freetype_gl_atlas.cpp
    nametag_system.h
    nametag_system.cpp
)

add_library(engine_graphics STATIC
    ${GRAPHICS_MAIN}
    ${GRAPHICS_API}
    ${GRAPHICS_API_OPENGL}
    ${GRAPHICS_PIPELINE}
    ${GRAPHICS_PASSES}
)

target_include_directories(engine_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_graphics PUBLIC
    engine_scene_core
    engine_core
    engine_project
    engine_assets
    engine_network
    yaml-cpp
    engine_external_entt
    glm
    engine_external_stb
    engine_external_imgui
    engine_external_glad
    assimp
    engine_freetype_gl
)

if(TARGET engine_pch)
    target_link_libraries(engine_graphics PRIVATE engine_pch)
endif()
