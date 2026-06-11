cmake_minimum_required(VERSION 3.31)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Windows SDK Version (must be set before project())
if(WIN32)
    set(WINDOWS_SDK_VERSION "10.0" CACHE STRING "Windows SDK Version")
    set(CMAKE_SYSTEM_VERSION ${WINDOWS_SDK_VERSION})
endif()

project(ChainedDecos LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)


# Options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(CH_CI "Set to ON for CI environments" OFF)

if(CH_CI)
    add_compile_definitions(CH_CI)
endif()

# Warning Settings
option(DISABLE_ALL_WARNINGS "Disable all compiler warnings" OFF)
option(ENABLE_WARNINGS "Enable compiler warnings" OFF)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# Optional: ccache support for faster rebuilds
# Install ccache: choco install ccache (Windows) or apt install ccache (Linux)
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


# Dependencies
include(cmake/CompilerSettings.cmake)
include(cmake/Dependencies.cmake)
include(cmake/ProjectHelpers.cmake)

# Useful definitions
if(UNIX AND NOT APPLE)
    add_compile_definitions(_GNU_SOURCE)
endif()

if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif()

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" PROJECT_ROOT_DIR_ESCAPED)
add_compile_definitions(PROJECT_ROOT_DIR="${PROJECT_ROOT_DIR_ESCAPED}")

# Standardize output directories for all targets
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_subdirectory(engine)
add_subdirectory(scripting)

# Build Game Projects
add_subdirectory(game/chaineddecos)
add_subdirectory(editor)
add_subdirectory(runtime)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

