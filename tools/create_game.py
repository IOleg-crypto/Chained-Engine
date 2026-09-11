#!/usr/bin/env python3
"""Scaffolds a new game project under game/<name>/ with boilerplate files.

Usage:
    python tools/create_game.py <GameName>
    python tools/create_game.py MyGame --csproj assets/scripts/MyGame.Scripts.csproj

Generated:
    game/<name>/
    ├── CMakeLists.txt          # chained_add_game() boilerplate
    ├── <Name>.chproject        # project metadata (YAML)
    ├── src/
    │   └── main.cpp            # CreateApplication entry point
    └── assets/
        └── scripts/            # empty, for C# scripts
"""

import argparse
import re
import sys
from pathlib import Path


# ── CMakeLists.txt template ──
# We use PYTHON_PLACEHOLDER syntax (uppercase with underscores) to avoid
# clashes with CMake ${VAR} syntax and Python {var} format strings.
# After .substitute(), we do .replace() to turn them into real CMake syntax.
_TEMPLATE_CMAKELISTS = """\
# Game: $PYDISPLAY_NAME

chained_add_game($PYTARGET_NAME
    PROJECT_GAME $PYGAME_DIR
    $PYCSHARP_LINE)

# Copy game assets (only if source files changed)
set(GAME_ASSET_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(GAME_ASSET_DST_DIR "$<TARGET_FILE_DIR:$PYTARGET_NAME>Exe>/assets")

add_custom_command(TARGET $PYTARGET_NAME Exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_ASSET_DST_DIR}"
    COMMAND ${CMAKE_COMMAND} -DSOURCE="${GAME_ASSET_SRC_DIR}" -DDEST="${GAME_ASSET_DST_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfDifferent.cmake"
    COMMENT "Syncing game assets from ${GAME_ASSET_SRC_DIR} to ${GAME_ASSET_DST_DIR}..."
)

# Sync engine resources (shaders, fonts, icons, config) to game output
if(COMMAND ch_add_resource_sync)
    ch_add_resource_sync($PYTARGET_NAME Exe)
endif()
"""

# ── main.cpp template ──
_TEMPLATE_MAIN_CPP = """\
#include "engine/app/entry_point.h"
#include "engine/core/platform.h"
#include "engine/runtime/runtime_layer.h"

namespace Chained
{
Application* CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationSpecification spec;
    spec.Name = "$PYDISPLAY_NAME";
    spec.CommandLineArgs = args;
    spec.EnableScripting = true;
    spec.EngineRoot = Platform::GetExecutableDirectory();
    spec.WorkingDirectory = Platform::GetExecutableDirectory().string();

    // Resolve project path: first CLI arg or default
    std::filesystem::path projectPath;
    for (int i = 0; i < args.Count; ++i)
    {
        std::string arg = args.Args[i];
        if (arg.ends_with(".chproject"))
        {
            projectPath = arg;
            break;
        }
    }

    if (projectPath.empty() || !std::filesystem::exists(projectPath))
    {
        projectPath = std::filesystem::path(spec.WorkingDirectory) / (spec.Name + ".chproject");
    }

    auto* app = new Application(spec);
    app->PushLayer(std::make_unique<RuntimeLayer>(projectPath.string()));
    return app;
}
} // namespace Chained
"""

# ── .chproject template ──
# Uses PY* placeholders similarly
_TEMPLATE_CHPROJECT = """\
Project:
  Name: $PYDISPLAY_NAME
  IconPath: resources/icons/chaineddecos.jpg
  StartScene: scenes/start_menu.chscene
  AssetDirectory: assets
  ActiveScene: assets/scenes/untitled.chscene
  Physics:
    Gravity: 9.8
    FixedTimestep: 0.02
  Animation:
    TargetFPS: 60
  Render:
    ShadowResolution: 2048
    EnableShadows: true
    AntiAliasingSamples: 4
  Mesh:
    ImportMaterials: true
    CalculateTangents: true
    FlipUVs: true
  Window:
    Width: 1920
    Height: 1080
    VSync: false
  Audio:
    MasterVolume: 1
    MusicVolume: 1
    SFXVolume: 1
  Runtime:
    Fullscreen: false
    ShowStats: true
    EnableConsole: false
    TargetFPS: 0
  Scripting:
    ModuleName: $PYSCRIPTS_DLL
    ModuleDirectory: assets/bin
    AutoLoad: true
  Export:
    Mode: 1
    ZipThreshold: 0.3
    DataVersion: 0
  BuildConfig: 0
"""


def main():
    parser = argparse.ArgumentParser(description="Create a new game project scaffold.")
    parser.add_argument("name", help="Game name (e.g. MyGame, ChainedDecos)")
    parser.add_argument(
        "--csproj",
        default=None,
        help="Relative path to C# project (e.g. assets/scripts/MyGame.Scripts.csproj)",
    )
    args = parser.parse_args()

    display_name = args.name
    # Derive CMake-safe target name (replace non-alphanumeric with underscore)
    target_name = re.sub(r"[^A-Za-z0-9_]", "_", display_name)
    game_dir = display_name.lower()

    root = Path(__file__).resolve().parent.parent / "game" / game_dir

    if root.exists():
        print(f"Error: {root} already exists.", file=sys.stderr)
        sys.exit(1)

    # Create directory structure
    (root / "assets" / "scripts").mkdir(parents=True)

    # CMakeLists.txt — substitute placeholders then replace CMake syntax
    csproj_line = f'CSHARP_PROJECT "{args.csproj}"\n    ' if args.csproj else ""
    cmake_content = _TEMPLATE_CMAKELISTS.replace(
        "$PYDISPLAY_NAME", display_name
    ).replace("$PYTARGET_NAME", target_name).replace("$PYGAME_DIR", game_dir).replace(
        "$PYCSHARP_LINE", csproj_line
    )
    (root / "CMakeLists.txt").write_text(cmake_content)

    # src/main.cpp
    main_cpp = _TEMPLATE_MAIN_CPP.replace("$PYDISPLAY_NAME", display_name)
    (root / "src" / "main.cpp").write_text(main_cpp)

    # .chproject — substitute placeholders then replace CMake syntax
    scripts_dll = f"{target_name}.Scripts.dll"
    chproject_content = _TEMPLATE_CHPROJECT.replace(
        "$PYDISPLAY_NAME", display_name
    ).replace("$PYSCRIPTS_DLL", scripts_dll)
    (root / f"{display_name}.chproject").write_text(chproject_content)

    print(f"Created game project: {root}")
    print(f"  CMakeLists.txt      — chained_add_game({target_name})")
    print(f"  src/main.cpp        — CreateApplication entry point")
    print(f"  assets/scripts/     — for C# scripts")
    print(f"  {display_name}.chproject — project metadata")
    print()
    print("Add to build:")
    print(f'  cmake --preset windows-clang -DCH_ACTIVE_GAME={game_dir}')


if __name__ == "__main__":
    main()