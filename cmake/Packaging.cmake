# ── Packaging Configuration (CPack) ──────────────────────────────────────────

set(CPACK_PACKAGE_NAME "ChainedEngine")
set(CPACK_PACKAGE_VENDOR "IOleg")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Chained Engine - 3D Game Engine, Editor & SDK")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/IOleg-crypto/Chained-Engine")
set(CPACK_PACKAGE_CONTACT "IOleg")

if(NOT DEFINED CPACK_PACKAGE_VERSION)
    set(CPACK_PACKAGE_VERSION_MAJOR 0)
    set(CPACK_PACKAGE_VERSION_MINOR 1)
    set(CPACK_PACKAGE_VERSION_PATCH 0)
    set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
endif()

# License & Readme files
if(EXISTS "${CMAKE_SOURCE_DIR}/license")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/license")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/readme.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/readme.md")
endif()

# Component packaging support
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# Component definitions & descriptions
set(CPACK_COMPONENT_EDITOR_DISPLAY_NAME "Chained Editor")
set(CPACK_COMPONENT_EDITOR_DESCRIPTION "Stand-alone editor for building 3D games and editing scenes.")
set(CPACK_COMPONENT_EDITOR_GROUP "Applications")

set(CPACK_COMPONENT_GAME_DISPLAY_NAME "Chained Decos Game")
set(CPACK_COMPONENT_GAME_DESCRIPTION "Standalone game binary and assets.")
set(CPACK_COMPONENT_GAME_GROUP "Applications")

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Engine Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Core runtime libraries, assets, and engine resources.")
set(CPACK_COMPONENT_RUNTIME_GROUP "Runtime")

set(CPACK_COMPONENT_SDK_DISPLAY_NAME "Chained Engine SDK")
set(CPACK_COMPONENT_SDK_DESCRIPTION "C++ headers, libraries, and scripting tools to develop with Chained Engine.")
set(CPACK_COMPONENT_SDK_GROUP "Development")

# Platform-specific packaging generators
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_SOURCE_GENERATOR "ZIP")
    
    # NSIS Installer configuration
    set(CPACK_NSIS_DISPLAY_NAME "Chained Engine")
    set(CPACK_NSIS_PACKAGE_NAME "ChainedEngine")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_SOURCE_GENERATOR "TGZ")
    set(CPACK_SET_DESTDIR ON)
    
    # Debian packaging metadata (.deb for Ubuntu, Debian, Linux Mint, Pop!_OS)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "IOleg <contact@chainedengine.dev>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/IOleg-crypto/Chained-Engine")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # RPM packaging metadata (.rpm for Fedora, RHEL, openSUSE)
    set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/IOleg-crypto/Chained-Engine")
    set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

# Package file naming pattern
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")

include(CPack)
