# pack dependency
if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/pack/CMakeLists.txt")
    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    set(PACK_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    add_subdirectory("${CMAKE_SOURCE_DIR}/thirdparty/pack"
        "${CMAKE_BINARY_DIR}/vendor/pack" EXCLUDE_FROM_ALL)

    # GCC 14+ treats -Wincompatible-pointer-types as error.
    # mpio/source/os.c passes char** to _spawvp() which expects const char* const*.
    if(TARGET mpio-static AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(mpio-static PRIVATE -Wno-error=incompatible-pointer-types)
    endif()
endif()