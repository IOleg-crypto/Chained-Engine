cmake_minimum_required(VERSION 3.31)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Enforce static CRT (/MT) globally — must be set before project() or early
# so that third-party add_subdirectory calls don't override it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Windows SDK Version (must be set before project())
if(WIN32)
    set(WINDOWS_SDK_VERSION "10.0" CACHE STRING "Windows SDK Version")
    set(CMAKE_SYSTEM_VERSION ${WINDOWS_SDK_VERSION})
endif()

project(ChainedDecos LANGUAGES CXX C)



option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(CH_CI "Set to ON for CI environments" OFF)
option(CH_NETWORKING "Enable networking features in the editor" ON)

if(CH_CI)
    add_compile_definitions(CH_CI)
endif()

# Global library definitions
# GLM behavior macros live here (not in engine_pch.h) so every TU — including
# NO_PCH modules — sees the same GLM configuration.
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_ENABLE_EXPERIMENTAL)

# Enable logging only in Debug and RelWithDebInfo configurations
add_compile_definitions($<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:CH_ENABLE_LOGGING>)

# Embed build preset name and configuration so the exporter knows exactly which build was used
# e.g. CH_BUILD_PRESET="windows-clang-debug", CH_BUILD_CONFIG="Debug"
add_compile_definitions($<$<CONFIG:Debug>:CH_BUILD_CONFIG="Debug">)
add_compile_definitions($<$<CONFIG:Release>:CH_BUILD_CONFIG="Release">)
add_compile_definitions($<$<CONFIG:RelWithDebInfo>:CH_BUILD_CONFIG="RelWithDebInfo">)
if(CMAKE_BUILD_PRESET)
	add_compile_definitions(CH_BUILD_PRESET="${CMAKE_BUILD_PRESET}")
endif()


# Optional: ccache support for faster rebuilds
option(USE_CCACHE "Use ccache if available" ON)
if(USE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    else()
        message(STATUS "ccache not found - install with: choco install ccache")
    endif()
endif()

# Note: do NOT add /INCREMENTAL here. MSVC enables it by default in Debug;
# forcing it globally conflicts with /OPT:ICF in Release (LNK4075).

# Dependencies
include(cmake/CompilerSettings.cmake)
include(cmake/Dependencies.cmake)
include(cmake/ProjectHelpers.cmake)

# Find Python interpreter
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CH_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "Python interpreter used for build helper scripts" FORCE)

set(CH_ACTIVE_GAME "chaineddecos" CACHE STRING "Active game project to build")
set_property(CACHE CH_ACTIVE_GAME PROPERTY STRINGS chaineddecos testproject)

# Source directories for dev mode — runtime reads from source tree instead of copying
set(CH_SOURCE_RESOURCES_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH "Engine resources source directory (dev mode)")
set(CH_SOURCE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/game/${CH_ACTIVE_GAME}/assets" CACHE PATH "Game assets source directory (dev mode)")

# Pass source directories as compile definitions so the runtime can read from source tree
add_compile_definitions(CH_SOURCE_RESOURCES_DIR="${CH_SOURCE_RESOURCES_DIR}")
add_compile_definitions(CH_SOURCE_ASSETS_DIR="${CH_SOURCE_ASSETS_DIR}")

if(UNIX AND NOT APPLE)
    add_compile_definitions(_GNU_SOURCE)
endif()

if(MINGW)
    # Serialize link steps under MinGW. Ninja otherwise links several heavy
    # Debug targets concurrently, and each linker process holds large archives
    # in memory at once — the combined footprint is what exhausts RAM. A
    # single-slot link pool keeps compilation fully parallel but links one
    # target at a time.
    set_property(GLOBAL PROPERTY JOB_POOLS link_pool=1)
    set(CMAKE_JOB_POOL_LINK link_pool)
endif()

add_subdirectory(engine)

# ── Build Game Projects ──────────────────────────────────────────────────────
file(GLOB GAME_PROJECT_LIST RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/game" "game/*")
foreach(GAME_PROJECT_DIR ${GAME_PROJECT_LIST})
    set(GAME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/game/${GAME_PROJECT_DIR}")
    if(IS_DIRECTORY "${GAME_PATH}" AND EXISTS "${GAME_PATH}/CMakeLists.txt")
        if(NOT CH_ACTIVE_GAME OR CH_ACTIVE_GAME STREQUAL GAME_PROJECT_DIR)
            add_subdirectory("game/${GAME_PROJECT_DIR}")
        endif()
    endif()
endforeach()

add_subdirectory(editor)
# ── Engine Tools ─────────────────────────────────────────────────────────────
# ── Google Tests for engine
enable_testing()
add_subdirectory(tests)

# ── Resource Sync Targets ────────────────────────────────────────────────────
add_custom_target(sync-resources
    COMMAND ${CH_PYTHON_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/sync_resources.py"
            sync-resources
            --root "${CMAKE_SOURCE_DIR}"
            --bin  "${CMAKE_BINARY_DIR}/bin"
            --all-configs
    COMMENT "Syncing resources to all configs"
    VERBATIM
)

add_custom_target(sync-scripts
    COMMAND ${CH_PYTHON_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/sync_scripts.py"
            --build-dir "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
            --game-dir  "${CMAKE_SOURCE_DIR}/game/chaineddecos"
    COMMENT "Syncing C# scripts to game assets"
    VERBATIM
)

# ── Packaging (CPack) ────────────────────────────────────────────────────────
include(cmake/Packaging.cmake)
