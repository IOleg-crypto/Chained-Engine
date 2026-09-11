
# ChainedEngine

## Custom C++/C# Game Engine with Editor, OpenGL Renderer, and Managed Scripting

[![C++23](https://img.shields.io/badge/language-C%2B%2B23-blue?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![Workflow Status](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml/badge.svg?branch=opengl)
[![Linux](https://img.shields.io/github/actions/workflow/status/IOleg-crypto/Chained-Engine/ci.yml?branch=opengl&job=Linux&label=Linux)](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/IOleg-crypto/Chained-Engine/ci.yml?branch=opengl&job=Windows&label=Windows)](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml)
[![OpenGL](https://img.shields.io/badge/graphics-OpenGL%204.3%2B-red?logo=opengl)](https://www.khronos.org/opengl/)

ChainedEngine is a modular C++23 game engine with editor tooling, runtime packaging, ECS architecture, physics, OpenGL 4.3+ rendering, and managed C# gameplay scripting via Coral. Ships with a parkour game **Chained Decos**.

![Game Screenshot](https://i.imgur.com/MLIxRhB.png)

> [!NOTE]
> Active development is ongoing. Features and workflows continue to evolve, but this README is maintained to reflect the current repository state.

> [!IMPORTANT]
> **Branch strategy:** The [`opengl`](https://github.com/IOleg-crypto/Chained-Engine/tree/opengl) branch is the active development branch where new features land first (newer OpenGL renderer improvements, editor features, networking changes). The [`main`](https://github.com/IOleg-crypto/Chained-Engine/tree/main) branch is the **stable** branch — it receives merged, tested changes from `opengl`. For everyday use and builds, prefer `main`. To follow cutting-edge development, use `opengl`.

## Table of Contents

- [Overview](#overview)
- [Download](#download)
- [Quick Start](#quick-start)
- [Git LFS](#git-lfs)
- [Build](#build)
- [Run](#run)
- [Working with Projects](#working-with-projects)
- [Project Structure](#project-structure)
- [Dependencies](#dependencies)
- [Prerequisites](#prerequisites)
- [Testing](#testing)
- [CI/CD](#cicd)
- [Troubleshooting](#troubleshooting)
- [Known Issues](#known-issues)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Overview

Chained Decos and Chained Engine target Windows and Linux.

- OpenGL 4.3+ rendering pipeline with PBR, fog, and shadow mapping
- ECS-driven scene model using EnTT
- YAML-based project and scene serialization
- Editor with hierarchy/inspector/panels and in-editor play mode
- Managed C# gameplay scripting through Coral (.NET/CoreCLR)
- UDP networking with client/server model, automated UPnP port forwarding, STUN WAN discovery, Hairpin NAT detection, and encryption
- Automatic high-performance discrete GPU offloading for dual-GPU laptops (Windows & Linux)
- Project export pipeline with compressed asset packs (ZSTD)
- Visual Animation Graph system (`.chag`)

> **Inspiration:** ChainedEngine is inspired by [Hazel](https://github.com/TheCherno/Hazel) by TheCherno, with significant custom additions — C# scripting via Coral, Jolt physics, animation graph system, project export pipeline, and multi-platform support.

![Editor Screenshot 1](https://i.imgur.com/jey25o0.png)
![Editor Screenshot 2](https://i.imgur.com/VMhs9Zm.jpeg)

## Download

### Chained Decos (Game)
- [Windows / Linux](https://www.mediafire.com/folder/upu4y995ivl26/releases) — pre-built binaries

### ChainedEngine (Editor)
- [GitHub Releases](https://github.com/IOleg-crypto/Chained-Engine/releases) — Windows + Linux builds

## Quick Start

**Clone (fast — code only, no assets):**
```bash
GIT_LFS_SKIP_SMUDGE=1 git clone --recurse-submodules https://github.com/IOleg-crypto/Chained-Engine.git
cd Chained-Engine
git submodule update --init --recursive
git lfs pull  # optional: download all assets
```

**Clone (full — with all assets):**
```bash
git clone --recurse-submodules https://github.com/IOleg-crypto/Chained-Engine.git
cd Chained-Engine
git submodule update --init --recursive
```

**Configure + Build + Run:**
```bash
# Linux (Clang)
cmake --preset linux-clang
cmake --build --preset linux-clang-debug --parallel
./build/linux-clang/bin/Debug/ChainedEditor

# Windows (LLVM Clang / MSYS2)
cmake --preset windows-clang
cmake --build --preset windows-clang-debug --parallel
.\build\windows-clang\bin\Debug\ChainedEditor.exe

# Windows (MSVC Ninja)
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug --parallel

# Windows (VS 2026 .sln)
cmake --preset windows-vs2026
```

> **Editor play mode:** Press PLAY to enter simulation and capture cursor. Press Escape to return to editor interaction.

## Git LFS

The repository uses **Git LFS** for large binary assets (models, textures, skyboxes, audio).

### Selective Asset Download

After cloning, download only what you need:

```bash
# All LFS files
git lfs pull

# Only game models
git lfs pull --include="game/chaineddecos/assets/models/**"

# Only textures
git lfs pull --include="game/chaineddecos/assets/models/textures/**"

# Only skyboxes
git lfs pull --include="game/chaineddecos/assets/skyboxes/**"

# Specific file type
git lfs pull --include="*.glb"
```

### Speed Up Transfers

```bash
git config lfs.concurrenttransfers 16
```

## Build

### Presets

| Preset | Generator | Compiler | Configuration | Use case |
| :--- | :--- | :--- | :--- | :--- |
| `windows-clang` | Ninja Multi-Config | Clang 18+ (LLVM / MSYS2) | Debug / Release | Primary dev (Windows) |
| `windows-msvc` | Ninja Multi-Config | MSVC (cl) | Debug / Release | MSVC Ninja builds |
| `windows-vs2026`| Visual Studio 18 2026 | MSVC | Debug / Release | VS solution / CI |
| `linux-clang` | Ninja Multi-Config | Clang 18+ | Debug / Release | Primary dev (Linux CI) |
| `linux-gcc` | Ninja Multi-Config | GCC 13+ | Debug / Release | Linux GCC |
| `windows-gcc` | Ninja Multi-Config | GCC (MinGW-w64) | Debug / Release | MinGW builds |

**Key CMake variables:**
- `CH_ACTIVE_GAME` — `chaineddecos` (default) or `testproject`. Build-time only. Switching requires reconfigure.
- `BUILD_TESTS` — ON by default.
- `CH_ENGINE_SHARED` — OFF by default (static engine).

### Clang & VS Code / clangd Setup

For the best IntelliSense and code navigation experience when using Clang:
1. Install the **clangd** extension in VS Code (disable the default Microsoft C/C++ IntelliSense engine).
2. Configure `.vscode/settings.json` to point to the compile commands database generated by the preset:
```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build/windows-clang",
    "--background-index",
    "--header-insertion=never"
  ]
}
```

## Run

Binaries are generated under `build/{preset}/bin/{Debug|Release}/` (or `build/{preset}/bin/` on single-config generators):

```bash
# Editor
./build/linux-clang/bin/Debug/ChainedEditor
.\build\windows-clang\bin\Debug\ChainedEditor.exe

# Runtime
./build/linux-clang/bin/Debug/ChainedRuntime path/to/project.chproject
.\build\windows-clang\bin\Debug\ChainedRuntime.exe --project path\to\project.chproject --name "My Runtime" --width 1600 --height 900
```

Runtime CLI: `--project` / `-p`, `--name`, `--width`, `--height`.

## Working with Projects

The engine supports multiple game projects under `game/`. Currently:
- **`chaineddecos`** — the main parkour game
- **`testproject`** — a lightweight sandbox for testing features

### Switching the Active Game

```bash
cmake -S . -B build/windows-clang -DCH_ACTIVE_GAME=testproject
```

Or in VS Code: Command Palette → `CMake: Edit CMake Cache (UI)` → change `CH_ACTIVE_GAME`.

### Creating a New Project

Use the scaffolding script:
```bash
python tools/create_game.py MyGame
python tools/create_game.py MyGame --csproj assets/scripts/MyGame.Scripts.csproj
```

This creates `game/mygame/` with:
- `CMakeLists.txt` — wired into the build via auto-discovery
- `src/main.cpp` — `CreateApplication` entry point
- `MyGame.chproject` — project metadata
- `assets/scripts/` — for C# scripts

New games are auto-discovered by the root `CMakeLists.txt` — any directory under `game/` with a `CMakeLists.txt` is included when `CH_ACTIVE_GAME` matches.

### Editor‑created projects

The editor (**Project → New Project**) now also generates CMake build scaffolding:

- `{project}/CMakeLists.txt` — `chained_add_game()` boilerplate
- `{project}/src/main.cpp` — `CreateApplication` entry point

For a standalone build, move the project directory under `game/` (e.g. `game/mygame/`) so it is auto‑discovered by the root `CMakeLists.txt`.

### Project Configuration (`.chproject`)

Each game has a YAML metadata file defining its entry scene, physics, rendering, and window settings. See [User Guide](docs/USER_GUIDE.md) for the full reference.

## Project Structure

- `engine/` — core engine modules (graphics, scene, physics, audio, platform, assets, networking)
- `editor/` — ChainedEditor application and editor panels/tools
- `runtime/` — ChainedRuntime application and runtime layer
- `engine/scripting/` — script host, glue bindings, and managed build integration
- `game/chaineddecos/` — main game project
- `game/testproject/` — alternate sandbox project
- `tests/` — native C++ tests (GoogleTest)
- `thirdparty/` — third-party dependencies (git submodules)
- `tools/` — build scripts, glue code generator, resource sync

## Dependencies

| Library | Purpose |
| :--- | :--- |
| EnTT | ECS framework |
| Assimp | 3D model import |
| Coral | C#/C++ interop |
| ImGui + ImGuizmo | Editor UI |
| GLFW + GLAD | Window/OpenGL |
| GLM | Math library |
| yaml-cpp | YAML serialization |
| GoogleTest | Unit/integration tests |
| JoltPhysics | Physics simulation |
| zstd + pack (cfnptr) | Asset pack compression |
| miniaudio | Audio |
| spdlog | Logging |
| stb | Image loading |
| cereal | Binary serialization |
| reflect-cpp | Runtime reflection |
| ENet | UDP networking |
| libsodium | Encryption (xchacha20poly1305) |
| miniupnpc | UPnP port forwarding |

Always init submodules before building:
```bash
git submodule update --init --recursive
```

## Prerequisites

| Tool | Version | Notes |
| :--- | :--- | :--- |
| CMake | 3.31+ | Required by top-level CMakeLists |
| Compiler | C++20 / C++23 | Clang 18+ (LLVM / MSYS2 / Linux), MSVC (VS2022+), or GCC 13+ |
| Ninja | Latest | Recommended for fast parallel builds |
| .NET SDK | 9.0.x | Required for Coral managed C# scripting |
| Graphics Driver | OpenGL 4.3+ | Needed for rendering |

**Windows Toolchain (LLVM Clang + Ninja + .NET 9 SDK):**
```bash
# Via Winget:
winget install LLVM.LLVM Ninja-build.Ninja Microsoft.DotNet.SDK.9

# Or via Chocolatey:
choco install llvm ninja ccache dotnet-9.0-sdk

# Or via MSYS2 (MINGW64):
pacman -S mingw-w64-x86_64-clang mingw-w64-x86_64-lld ninja
```

**Linux Packages (Ubuntu / Debian reference):**
```bash
# Core build dependencies + .NET 9 SDK
sudo apt-get install -y build-essential cmake ninja-build clang lld \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libasound2-dev libglu1-mesa-dev \
  pkg-config libgtk-3-dev libdrm-dev libgbm-dev \
  xvfb libxkbcommon-x11-0 libgl1-mesa-dri mesa-utils dotnet-sdk-9.0
```

### Building on WSL2 (Windows Subsystem for Linux)

ChainedEngine builds and runs under **WSL2 + Ubuntu 24.04 / 22.04** with hardware-accelerated OpenGL 4.3+ via WSLg (Mesa D3D12 backend translating OpenGL calls directly to DirectX 12 on your Windows GPU).

#### 1. Configure WSL2 for NTFS Mounts (Crucial Step)

By default, WSL2 mounts Windows drives without Linux file permissions and `utime()` support, which causes CMake and MSBuild to fail with `Operation not permitted` (MSB3374). To allow building directly on your Windows drive (e.g. `D:\`), enable `metadata` in `/etc/wsl.conf`:

```bash
sudo bash -c 'cat << "EOF" > /etc/wsl.conf
[boot]
systemd=true

[automount]
enabled = true
options = "metadata,umask=22,fmask=11"
mountFsTab = true
EOF'
```

*Apply the change:* In Windows PowerShell run `wsl --shutdown`, then reopen your WSL2 terminal. (Or remount immediately: `sudo mount -o remount,metadata,umask=22,fmask=11 /mnt/d`).

#### 2. Install Required Dependencies

```bash
# Core build dependencies + X11 + OpenGL + Clang 18 + Tools
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build clang lld clang-tools-18 \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libxext-dev libgl1-mesa-dev libglu1-mesa-dev libasound2-dev \
  mesa-utils zlib1g-dev pkg-config python3 dotnet-sdk-9.0

# Ensure clang-scan-deps and dotnet paths are globally available
sudo ln -sf /usr/bin/clang-scan-deps-18 /usr/bin/clang-scan-deps
sudo ln -sf ~/.dotnet/dotnet /usr/local/bin/dotnet 2>/dev/null || true

# Coral CoreCLR discovery requires .NET in /usr/share/dotnet
if [ -d "$HOME/.dotnet" ] && [ ! -d "/usr/share/dotnet" ]; then
  sudo ln -sf "$HOME/.dotnet" /usr/share/dotnet
fi
```

#### 3. Configure & Build directly on Drive D:

```bash
cd "/mnt/d/gitnext/Chained Decos"

# Configure with Linux Clang preset (builds into build/linux-clang/)
cmake --preset linux-clang

# Build all targets in parallel
cmake --build build/linux-clang --config Debug --parallel

# (Optional) Build only the Editor
cmake --build build/linux-clang --config Debug --parallel --target ChainedEditor
```

#### 4. Run with Hardware Acceleration (WSLg)

Configure hardware acceleration in `~/.bashrc` to use your dedicated GPU via Mesa D3D12:

```bash
echo 'export GALLIUM_DRIVER=d3d12' >> ~/.bashrc
echo 'export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA' >> ~/.bashrc
source ~/.bashrc

# Run the editor
./build/linux-clang/bin/Debug/ChainedEditor
```

Verify GPU acceleration with `glxinfo | grep "OpenGL renderer"` — it should show `D3D12 (NVIDIA GeForce ...)`.

---

### Dual-Platform Development Workflow (Windows + WSL2)

You can work on both Windows and Linux simultaneously from the **exact same project directory** on your Windows drive (e.g. `D:\gitnext\Chained Decos`):

```
D:\gitnext\Chained Decos\
├── build\
│   ├── windows-clang\   <── Native Windows binaries (.exe, .dll)
│   └── linux-clang\     <── Native Linux / WSL2 ELF binaries
├── engine\
│   └── scripting\
│       └── managed\
│           └── managed-obj\ <── Isolated C# compilation intermediates
```

#### Why Dual-Platform Works Seamlessly Here:
1. **Isolated Build Directories:**
   - Windows builds into `build/windows-clang/` (run from Windows PowerShell)
   - WSL2 builds into `build/linux-clang/` (run from WSL2 Bash)
   - There are zero binary path collisions between the two platforms.
2. **C# Scripting Isolation:**
   - `Directory.Build.props` and `build_managed.py` automatically route MSBuild `obj/` outputs to isolated paths (`managed-obj/` on Linux, default on Windows) and exclude stale `obj/**` artifacts via `DefaultItemExcludes`. Windows MSBuild and Linux `dotnet` will never overwrite each other's assembly metadata or lock files.
3. **Cross-Platform Networking & Multiplayer Testing:**
   - Run the host game or editor in native Windows (`.\build\windows-clang\bin\Debug\ChainedEditor.exe`).
   - Simultaneously launch a client instance inside WSL2 (`./build/linux-clang/bin/Debug/ChainedDecos`) to test cross-platform replication, byte endianness, and packet sequencing locally on `127.0.0.1`!
4. **Recommended Daily Flow:**
   - **Primary Development & Scene Editing:** Use native Windows for the editor (zero input latency, direct WASAPI audio, full RTX performance).
   - **Cross-Platform Validation:** Keep a WSL2 terminal open to run `cmake --build build/linux-clang --parallel` to verify Linux compilation and run automated test suites (`ctest --test-dir build/linux-clang`).




## Testing

Native tests use GoogleTest + CTest, split into unit (fast, no engine runtime) and integration (full engine) targets.

```bash
# Run all tests
ctest --test-dir build/windows-clang -C Debug --output-on-failure

# Unit tests only
ctest --test-dir build/windows-clang -C Debug -L Unit --output-on-failure

# Integration tests only
ctest --test-dir build/windows-clang -C Debug -L Integration --output-on-failure
```

Managed (C#) tests require .NET 9.0.x SDK on PATH. Without it, the scripting target is silently skipped.

## CI/CD

CI workflow (`.github/workflows/ci.yml`) fans out to:
- **Format Check** (`format.yml`): `clang-format-18 --dry-run -Werror` on changed C++ files
- **Linux Builds** (`linux.yml`): Debug + Release under `xvfb` + Mesa software rendering
- **Windows Builds** (`windows.yml`): Debug + Release matrix (Clang, MSVC, GCC)

Debug builds use `-DENABLE_SANITIZERS=ON` (ASan + UBSan). CTest output is captured as JUnit XML.

Deploy workflow (`.github/workflows/deploy-sdk.yml`): triggered by `v*` tags, packages ChainedEditor + ChainedRuntime artifacts.

## Troubleshooting

- **Coral Status 3 / DotNetNotFound:** Install .NET 9.0 SDK or Runtime (`winget install Microsoft.DotNet.SDK.9`). Coral host strictly discovers `.NET 9.0.x` hostfxr in `host/fxr/9.*`.
- **Clang not found on Windows:** Ensure your LLVM bin directory (`C:\Program Files\LLVM\bin` or `C:\LLVM\bin`) is added to the system `PATH`.
- **Submodule errors:** `git submodule update --init --recursive`
- **Generator conflicts:** Reconfigure from a clean build folder when switching generator families
- **No managed build:** Ensure `dotnet` SDK (9.0.x) is on PATH
- **Linux headless:** Install packages from Prerequisites, use `xvfb` + Mesa
- **Stale files after `CH_ACTIVE_GAME` change:** Reconfigure the build directory, don't just rebuild
- **LFS files not downloading:** Run `git lfs pull` after cloning
- **Slow LFS downloads:** Increase concurrent transfers: `git config lfs.concurrenttransfers 16`
- **Multiplayer host connection issues:** If external players cannot connect, ensure UDP port 4588 (or your selected game port) is allowed in Windows Firewall (`New-NetFirewallRule -DisplayName "ChainedDecos UDP" -Direction Inbound -Protocol UDP -LocalPort 4588 -Action Allow`) or check UPnP settings on your router.

## Known Issues

- Font system needs rework for in-scene text and editor
- Some native tests are being reworked and may be skipped in CI
- Runtime and editor workflows are under active iteration
- Virtual file system is planned/in-progress
- **Runtime may have bugs and issues — known problems include font rendering, physics edge cases, and occasional crashes under specific scenarios**

## Documentation

Full guides and references:

| Document | Description |
| :--- | :--- |
| [User Guide](docs/USER_GUIDE.md) | Step-by-step: build, run, create scenes, write scripts, export |
| [Engine Architecture](docs/ARCHITECTURE.md) | Bootstrapping, system initialization, main loop |
| [Component Reference](docs/COMPONENTS.md) | All ECS components, adding new ones |
| [Scripting API](docs/SCRIPTING_API.md) | C# API reference: lifecycle, entities, input, UI |
| [Scripting Interop](docs/SCRIPTING_INTEROP.md) | C++/C# bridge internals |
| [Animation Graphs](docs/ANIMATION_GRAPHS.md) | Visual animation graph system tutorial |
| [Dual-Platform Guide](docs/DUAL_PLATFORM_GUIDE.md) | Full guide for Windows + WSL2 simultaneous development & setup |
| [Export Guide](docs/EXPORT.md) | Project packaging and distribution |
| [FAQ](docs/FAQ.md) | Common patterns and solutions |

## Contributing

- Open issues for bugs/regressions
- Submit pull requests for fixes and improvements
- Platform/build workflow improvements are especially helpful

## License

This project is licensed under MIT. See [license](license) for details.
