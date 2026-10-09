# ============================================================================
# Chained Engine - Tracy Profiler Dependency
# ============================================================================

option(CH_ENABLE_PROFILING "Enable Tracy Profiler" ON)

if(CH_ENABLE_PROFILING)
    set(TRACY_DIR "${CMAKE_SOURCE_DIR}/thirdparty/tracy")

    if(EXISTS "${TRACY_DIR}/public/TracyClient.cpp")
        add_library(engine_external_tracy STATIC
            "${TRACY_DIR}/public/TracyClient.cpp"
        )

        target_include_directories(engine_external_tracy PUBLIC
            "${TRACY_DIR}/public"
        )

        target_compile_definitions(engine_external_tracy PUBLIC
            TRACY_ENABLE
            TRACY_ON_DEMAND
            TRACY_NO_BROADCAST
        )

        if(WIN32)
            target_link_libraries(engine_external_tracy PUBLIC ws2_32 dbghelp)
        elseif(UNIX AND NOT APPLE)
            target_link_libraries(engine_external_tracy PUBLIC pthread dl)
        endif()

        add_library(Tracy::TracyClient ALIAS engine_external_tracy)
        message(STATUS "Tracy Profiler: Enabled (TRACY_ENABLE) via thirdparty/tracy")
    else()
        message(WARNING "Tracy Profiler enabled, but thirdparty/tracy submodule is missing. Run 'git submodule update --init --recursive'")
    endif()
else()
    message(STATUS "Tracy Profiler: Disabled")
endif()
