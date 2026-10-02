# pack dependency
if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/pack/CMakeLists.txt")
    # Ensure lz4_static is built without modifying third-party pack submodule
    if(NOT TARGET lz4_static)
        set(LZ4_DIR "${CMAKE_SOURCE_DIR}/thirdparty/pack/libraries/lz4")
        if(EXISTS "${LZ4_DIR}/build/cmake/CMakeLists.txt")
            set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
            add_subdirectory("${LZ4_DIR}/build/cmake" "${CMAKE_BINARY_DIR}/vendor/lz4" EXCLUDE_FROM_ALL)
        elseif(EXISTS "${LZ4_DIR}/lib/lz4.c")
            file(GLOB LZ4_SOURCES "${LZ4_DIR}/lib/*.c")
            add_library(lz4_static STATIC ${LZ4_SOURCES})
            target_include_directories(lz4_static PUBLIC
                $<BUILD_INTERFACE:${LZ4_DIR}/lib>
                $<INSTALL_INTERFACE:include>
            )
            target_compile_definitions(lz4_static PRIVATE XXH_NAMESPACE=LZ4_)
            set_target_properties(lz4_static PROPERTIES
                POSITION_INDEPENDENT_CODE ON
            )
            if(NOT TARGET lz4)
                add_library(lz4 INTERFACE)
                target_link_libraries(lz4 INTERFACE lz4_static)
            endif()
        endif()
    endif()

    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    set(PACK_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(PACK_BUILD_UTILITIES OFF CACHE BOOL "" FORCE)
    set(PACK_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    add_subdirectory("${CMAKE_SOURCE_DIR}/thirdparty/pack"
        "${CMAKE_BINARY_DIR}/vendor/pack" EXCLUDE_FROM_ALL)

    if(TARGET pack-static)
        if(TARGET lz4_static)
            target_link_libraries(pack-static PUBLIC lz4_static)
        endif()
        if(TARGET libzstd_static)
            target_link_libraries(pack-static PUBLIC libzstd_static)
        endif()
        if(TARGET mpio-static)
            target_link_libraries(pack-static PUBLIC mpio-static)
        endif()
    endif()

    if(TARGET libzstd_static AND NOT TARGET zstd::libzstd_static)
        add_library(zstd::libzstd_static ALIAS libzstd_static)
    endif()

    # GCC 14+ treats -Wincompatible-pointer-types as error.
    # mpio/source/os.c passes char** to _spawvp() which expects const char* const*.
    if(TARGET mpio-static AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(mpio-static PRIVATE -Wno-error=incompatible-pointer-types)
    endif()
endif()