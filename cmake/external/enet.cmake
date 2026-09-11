# ENet — Reliable UDP networking library
# Provides: client/server, reliable/unreliable channels, fragmentation.
# Single-header variant (enet.h with ENET_IMPLEMENTATION).
if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/enet")
    set(ENET_STATIC ON CACHE BOOL "" FORCE)
    set(ENET_SHARED OFF CACHE BOOL "" FORCE)
    set(ENET_TEST OFF CACHE BOOL "" FORCE)
    # Force IPv4-only sockets: ENet's default AF_INET6 dual-stack fails to
    # bind on some Windows systems. IPv4 is reliable everywhere we ship.
    set(ENET_IPV4_ONLY ON CACHE BOOL "" FORCE)

    add_subdirectory("${CMAKE_SOURCE_DIR}/thirdparty/enet" "${CMAKE_BINARY_DIR}/enet")

    if(TARGET enet_static)
        target_compile_definitions(enet_static PUBLIC ENET_IPV4_ONLY)
    endif()

    message(STATUS "enet: configured (static lib, IPv4-only)")
else()
    message(FATAL_ERROR "enet missing at ${CMAKE_SOURCE_DIR}/thirdparty/enet")
endif()
