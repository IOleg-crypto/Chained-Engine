# Chained Engine - Compiler Settings
# Extracted from root CMakeLists.txt for modularity

# Compiler-specific settings
if(MSVC)
    # MSVC-specific settings
    add_compile_options(
        $<$<CONFIG:Debug>:/Od> $<$<CONFIG:Debug>:/MTd>
        $<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/MT> $<$<CONFIG:Release>:/DNDEBUG>
        /Zi /EHsc
        /MP                  # Multi-processor compilation
        /Zc:preprocessor     # Modern preprocessor
        /Gm-                 # Disable minimal rebuild (it's slower)
        /utf-8               # Use UTF-8 character set
        /bigobj              # Allow large object files (required for many modules)
        
        # Dead Code Elimination: Function-Level Linking
        $<$<CONFIG:Release>:/Gy>
    )
    
    # Strip unused functions in Release
    add_link_options($<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)

    if(DISABLE_ALL_WARNINGS)
        add_compile_options(/W0)
    elseif(ENABLE_WARNINGS)
        add_compile_options(/W4 /permissive-)
        if(WARNINGS_AS_ERRORS)
            add_compile_options(/WX)
        endif()
    else()
        add_compile_options(/W1)
    endif()

    if(ENABLE_SANITIZERS)
        add_compile_options(/fsanitize=address)
    endif()

    # Level 2 Security Hardening
    add_compile_options(/guard:cf /GS)
    add_link_options(/DYNAMICBASE /NXCOMPAT /guard:cf)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang settings (includes AppleClang and clang-cl)
    if(MSVC)
        # clang-cl behaves like MSVC
        add_compile_options(/Zc:preprocessor /utf-8 /bigobj)
    else()
        add_compile_options(
            $<$<CONFIG:Debug>:-O0> $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-DNDEBUG>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>
        )

        if(MINGW)
            add_compile_options(-Wa,-mbig-obj)
        endif()

        # Dead Code Elimination linkage and binary stripping for Release build
        add_link_options(
            $<$<CONFIG:Release>:-Wl,--gc-sections>
            $<$<CONFIG:Release>:-s>
        )

        if(DISABLE_ALL_WARNINGS)
            add_compile_options(-w)
        elseif(ENABLE_WARNINGS)
            add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wmost -Wno-missing-braces -Wno-missing-field-initializers -Wno-attributes)
            if(WARNINGS_AS_ERRORS)
                add_compile_options(-Werror)
            endif()
        else()
            add_compile_options(-Wno-all)
        endif()
    endif()

    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address -fsanitize=undefined)
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()

    if(NOT WIN32)
        add_link_options(-Wl,-z,relro -Wl,-z,now)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC settings
    add_compile_options(
        $<$<CONFIG:Debug>:-O0> 
        $<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:Release>:-ffunction-sections>
        $<$<CONFIG:Release>:-fdata-sections>
    )

    if(MINGW)
        add_compile_options(-Wa,-mbig-obj)
        add_link_options("-fuse-ld=lld")
    endif()

    # Dead Code Elimination linkage
    add_link_options(
        $<$<CONFIG:Release>:-Wl,--gc-sections>
        $<$<CONFIG:Release>:-s>
    )

    if(DISABLE_ALL_WARNINGS)
        add_compile_options(-w)
    elseif(ENABLE_WARNINGS)
        add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers -Wno-attributes)
        if(WARNINGS_AS_ERRORS)
            add_compile_options(-Werror)
        endif()
    else()
        add_compile_options(-Wno-all)
    endif()

    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address -fsanitize=undefined)
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()

    if(NOT WIN32)
        add_link_options(-Wl,-z,relro -Wl,-z,now)
    endif()
endif()

# Platform-specific settings
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(_WIN32_WINNT=0x0601)  # Windows 7+
endif()

# Optimized Build Settings
option(ENABLE_UNITY_BUILD "Enable Unity Builds for faster compilation" OFF)
option(ENABLE_PCH "Enable Precompiled Headers for faster compilation" ON)
option(ENABLE_LTO "Enable Link-Time Optimization (IPO) for Release configurations" ON)
option(ENABLE_COVERAGE "Enable Code Coverage (GCC/Clang only)" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()
endif()

if(ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)
endif()

# Function to apply common engine optimizations to a target
function(apply_engine_optimizations target_name)
    if(ENABLE_PCH)
        target_precompile_headers(${target_name} PUBLIC "${CMAKE_SOURCE_DIR}/engine/core/engine_pch.h")
    endif()

    if(ENABLE_LTO)
        if(MINGW)
            set(ipo_supported OFF)
        else()
            include(CheckIPOSupported)
            check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
        endif()

        if(ipo_supported)
            set_property(TARGET ${target_name} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        elseif(ipo_output)
            message(WARNING "IPO/LTO is not supported by the not current compiler: ${ipo_output}")
        endif()
    endif()
endfunction()
