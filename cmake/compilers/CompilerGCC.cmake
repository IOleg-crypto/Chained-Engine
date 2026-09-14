add_compile_options(
    # Debug: Plain -g. We use -mstackrealign because MinGW GCC at -O0 sometimes 
    # fails to maintain the 16-byte stack alignment required by the Windows x64 ABI.
    # This causes Access Violations (AV) inside CoreCLR/hostfxr which uses SSE/AVX
    # instructions heavily during initialization and callbacks. LLD handles the
    # debug info memory pressure.
    $<$<CONFIG:Debug>:-O0> $<$<CONFIG:Debug>:-g> $<$<CONFIG:Debug>:-mstackrealign>
    $<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-DNDEBUG>
    # Section-per-symbol in all configs so --gc-sections can drop unused code and
    # shrink what the linker has to hold in memory.
    -ffunction-sections
    -fdata-sections
)

if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif()

if(CH_CI)
    # CI only runs Debug binaries to execute tests, never steps through them.
    # Full -g on GCC (especially MinGW) dominates compile and link time and
    # object size; -g1 keeps line tables for readable backtraces. Listed after
    # the -g above, so it wins (GCC takes the last debug-level flag).
    add_compile_options($<$<CONFIG:Debug>:-g1>)
endif()

# Prefer LLD over the default BFD ld: it is dramatically more memory-efficient
# and faster, which is what actually fixes the MinGW Debug link failures.
# Guard on availability so the configure step never fails on toolchains that
# ship without lld (e.g. a minimal Linux CI runner) — there we silently fall
# back to the default linker.
find_program(CH_LLD_LINKER NAMES lld ld.lld lld-link)
if(CH_LLD_LINKER)
    add_link_options(-fuse-ld=lld)
    message(STATUS "GCC: using LLD linker (${CH_LLD_LINKER})")
else()
    message(STATUS "GCC: lld not found, falling back to default linker (heavy Debug links may exhaust memory)")
endif()

# Note: -Wno-template-body is Clang-only; GCC does not need it (GCC-13+ ignores
# out-of-line template body errors by default in the contexts that trip Clang).

# Dead Code Elimination linkage and binary stripping.
# Enabled in all configs now that -ffunction-sections/-fdata-sections are global:
# dropping unused sections also reduces linker memory pressure in Debug.
add_link_options(
    -Wl,--gc-sections
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
