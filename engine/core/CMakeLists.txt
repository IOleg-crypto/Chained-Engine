add_library(engine_core OBJECT
        layer_stack.cpp
        profiler.cpp
        uuid.cpp
        window_factory.cpp
        input.cpp
        imgui_layer.cpp
        thread_pool.cpp
        application.cpp
        settings_manager.cpp
        dialogs.cpp
        filesystem_utils.cpp
        assets/asset_manager.cpp
        assets/asset.h
        assets/asset_manager.h
        assets/asset_loader.h

    # Platform specific implementations
    $<$<PLATFORM_ID:Windows>:../platform/windows/windows_window.cpp>
    $<$<PLATFORM_ID:Linux>:../platform/linux/linux_window.cpp>

        application.h
        imgui_layer.h
        base.h
        assert.h
        platform_detection.h
        thread_pool.h
        entry_point.h
        events.h
        input.h
        layer.h
        layer_stack.h
        log.h
        profiler.h
        timestep.h
        uuid.h
        window.h
        yaml.h
        settings_manager.h
        dialogs.h
        filesystem_utils.h
)

target_include_directories(engine_core PUBLIC
    ${ENGINE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include/imgui
    ${CMAKE_SOURCE_DIR}/include/imgui/backends
    ${CMAKE_SOURCE_DIR}/include/raylib/src/external/glfw/include
    ${CMAKE_SOURCE_DIR}/include/assimp/include
    ${CMAKE_BINARY_DIR}/include/assimp/include
)

# Core module uses yaml-cpp for some configurations
target_link_libraries(engine_core PUBLIC yaml-cpp raylib imguilib EnTT Coral.Native nfd assimp)

target_compile_definitions(engine_core PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# We apply optimizations and precompiled headers specific to core
if(COMMAND apply_engine_optimizations)
    apply_engine_optimizations(engine_core)
endif()
