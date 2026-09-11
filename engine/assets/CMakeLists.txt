
set (ASSET_LOADERS loaders/iasset_loader.h
    loaders/anim_graph_loader.h
    loaders/anim_graph_loader.cpp
    loaders/assimp_importer.h
    loaders/assimp_importer.cpp
    loaders/assimp_helpers.h
    loaders/pack_io.h
    loaders/pack_io.cpp
    loaders/material_extractor.h
    loaders/material_extractor.cpp
    loaders/animation_sampler.h
    loaders/animation_sampler.cpp
    loaders/audio_loader.h
    loaders/audio_loader.cpp
    loaders/environment_loader.h
    loaders/environment_loader.cpp
    loaders/font_loader.h
    loaders/font_loader.cpp
    loaders/material_loader.h
    loaders/material_loader.cpp
    loaders/model_loader.h
    loaders/model_loader.cpp
    loaders/shader_loader.h
    loaders/shader_loader.cpp
    loaders/stb_image_impl.h
    loaders/texture_loader.h
    loaders/texture_loader.cpp
)

set(ASSET_TYPES
    types/audio_asset.h
    types/environment_asset.h
    types/font_asset.h
    types/font_asset.cpp
    types/material_asset.h
    types/material_asset.cpp
    types/model_asset.h
    types/model_asset.cpp
    types/shader_asset.h
    types/texture_asset.h
    types/texture_asset.cpp
    types/animation_graph_asset.h
)

set(ASSET_MAIN
    asset.h
    asset_manager.h
    asset_manager.cpp
    asset_metadata.h
    asset_metadata.cpp
    asset_pack_store.h
    asset_pack_store.cpp
    asset_path_resolver.cpp
    asset_path_resolver.h
    asset_pack_store.cpp
    asset_pack_store.h
    model_data.h
)

add_library(engine_assets STATIC
    ${ASSET_MAIN}
    ${ASSET_LOADERS}
    ${ASSET_TYPES}
)
    

target_include_directories(engine_assets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_assets PUBLIC
    engine_core
    engine_project
    engine_graphics
    engine_external_glad
    engine_external_stb
    engine_external_miniaudio
    yaml-cpp
    assimp
)

if(TARGET pack-static)
    target_link_libraries(engine_assets PUBLIC pack-static)
endif()


if(TARGET engine_external_basisu_transcoder)
    target_link_libraries(engine_assets PUBLIC engine_external_basisu_transcoder)
endif()

if(TARGET engine_pch)
    target_link_libraries(engine_assets PRIVATE engine_pch)
endif()
