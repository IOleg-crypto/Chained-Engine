# ChainedEngine

## Custom C++/C# Game Engine with Editor, OpenGL Renderer, and Managed Scripting

[![C++23](https://img.shields.io/badge/language-C%2B%2B23-blue?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CI](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml/badge.svg?branch=opengl)](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml)
[![Linux](https://img.shields.io/github/actions/workflow/status/IOleg-crypto/Chained-Engine/ci.yml?branch=opengl&job=Linux&label=Linux)](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/IOleg-crypto/Chained-Engine/ci.yml?branch=opengl&job=Windows&label=Windows)](https://github.com/IOleg-crypto/Chained-Engine/actions/workflows/ci.yml)
[![OpenGL](https://img.shields.io/badge/graphics-OpenGL%204.3%2B-red?logo=opengl)](https://www.khronos.org/opengl/)

ChainedEngine is a modular C++23 game engine with editor tooling, runtime packaging, ECS architecture, Jolt physics, OpenGL 4.3+ rendering, and managed C# gameplay scripting via Coral (.NET 9). Ships with the parkour game **Chained Decos**.

![Game Screenshot](https://i.imgur.com/MLIxRhB.png)

> [!NOTE]
> Active development is ongoing. Features and workflows continue to evolve, but this README is maintained to reflect the current repository state.

> [!IMPORTANT]
> **Branch strategy:** The [`opengl`](https://github.com/IOleg-crypto/Chained-Engine/tree/opengl) branch is the active development branch where new features land first. The [`main`](https://github.com/IOleg-crypto/Chained-Engine/tree/main) branch is the **stable** branch receiving tested merges.

---

## Table of Contents

- [Overview](#overview)
- [Download](#download)
- [Quick Start](#quick-start)
- [Git LFS](#git-lfs)
- [Build Presets](#build-presets)
- [Project Structure](#project-structure)
- [Dependencies](#dependencies)
- [Testing](#testing)
- [CI/CD](#cicd)
- [Documentation](#documentation)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Overview

ChainedEngine and Chained Decos target **Windows** and **Linux**:

- **Rendering:** OpenGL 4.3+ pipeline with PBR materials, dynamic shadows, fog, post-processing, and material-aware instancing.
- **ECS & Scene Graph:** Fast EnTT-driven entity-component system with hierarchical transform updates.
- **Scripting:** High-performance C# scripting via Coral (.NET 9 CoreCLR), featuring hot-reloading, `[Autoload]` persistent global services, and `[AutoAttach]` tag-based discovery.
- **Physics:** Jolt Physics 3D backend with multithreaded simulation, raycasting, and fast BVH collision baking.
- **Networking:** Multi-channel threaded ENet UDP architecture with UPnP port mapping, LAN/WAN discovery, and client prediction / dead reckoning.
- **Editor:** ImGui + ImGuizmo desktop editor with inspector, asset browser, scene viewport, animation graph editor, and live play-mode.
- **Asset Pipeline:** Binary `.chasset` format, KTX2/BC7 texture compression, and ZSTD-compressed dictionary asset packs (`.pack`).

![Editor Screenshot 1](https://i.imgur.com/jey25o0.png)
![Editor Screenshot 2](https://i.imgur.com/VMhs9Zm.jpeg)

---

## Download

Pre-built binaries for **Chained Decos** (game) and **ChainedEngine** (editor) for Windows and Linux are available on [GitHub Releases](https://github.com/IOleg-crypto/Chained-Engine/releases).

---

## Quick Start

### 1. Clone the Repository

```bash
# Fast clone (code only, skips large Git LFS binary assets):
GIT_LFS_SKIP_SMUDGE=1 git clone --recurse-submodules https://github.com/IOleg-crypto/Chained-Engine.git
cd Chained-Engine
git submodule update --init --recursive
git lfs pull  # download game models, textures, and audio
```

### 2. Configure & Build

```bash
# Windows (LLVM Clang + Ninja)
cmake --preset windows-clang
cmake --build --preset windows-clang-debug --parallel

# Windows (MSVC)
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug --parallel

# Linux (Clang)
cmake --preset linux-clang
cmake --build --preset linux-clang-debug --parallel
```

### 3. Run

```bash
# Run Editor
./build/linux-clang/bin/Debug/ChainedEditor          # Linux
.\build\windows-clang\bin\Debug\ChainedEditor.exe   # Windows

# Run Game
./build/linux-clang/bin/Debug/ChainedDecos           # Linux
.\build\windows-clang\bin\Debug\ChainedDecos.exe    # Windows
```

> **Dual-Platform Development (Windows + WSL2):** You can build and run both native Windows and Linux binaries simultaneously from the same project folder. See the [Dual-Platform Guide](docs/DUAL_PLATFORM_GUIDE.md).

---

## Git LFS

The repository uses **Git LFS** for models, textures, skyboxes, and audio.

```bash
# Pull all assets
git lfs pull

# Or selectively pull only specific assets:
git lfs pull --include="game/chaineddecos/assets/models/**"
git lfs pull --include="game/chaineddecos/assets/skyboxes/**"

# Speed up transfers:
git config lfs.concurrenttransfers 16
```

---

## Build Presets

| Preset | Generator | Compiler | Configuration | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `windows-clang` | Ninja Multi-Config | Clang 18+ (LLVM / MSYS2) | Debug / Release | Primary development (Windows) |
| `windows-msvc` | Ninja Multi-Config | MSVC (`cl`) | Debug / Release | MSVC Ninja builds |
| `windows-vs2026`| Visual Studio 18 2026 | MSVC | Debug / Release | Visual Studio Solution |
| `linux-clang` | Ninja Multi-Config | Clang 18+ | Debug / Release | Primary development (Linux) |
| `linux-gcc` | Ninja Multi-Config | GCC 13+ | Debug / Release | Linux GCC builds |

Key CMake configuration options:
- `-DCH_ACTIVE_GAME=chaineddecos` (default) or `-DCH_ACTIVE_GAME=testproject`
- `-DBUILD_TESTS=ON` (enabled by default)
- `-DCH_ENGINE_SHARED=OFF` (static engine build)

For detailed project setup and creating new games, refer to the [User Guide](docs/USER_GUIDE.md).

---

## Project Structure

```
Chained-Engine/
├── docs/                # Comprehensive documentation & API references
├── editor/              # ChainedEditor desktop application and ImGui panels
├── engine/              # Core engine modules (graphics, scene, physics, assets, audio, networking)
│   └── scripting/       # C# script host, glue bindings, and Coral managed runtime
├── game/
│   ├── chaineddecos/    # Main parkour game project assets & scripts
│   └── testproject/     # Sandbox project for experimentation
├── resources/           # Built-in shaders, primitive models, icons, and UI fonts
├── tests/               # Native C++ tests (GoogleTest) and C# Managed tests (NUnit)
├── thirdparty/          # Vendored dependencies (submodules)
└── tools/               # Scaffolding, build helpers, and glue generators
```

---

## Dependencies

| Dependency | Purpose |
| :--- | :--- |
| **EnTT** | Entity-Component-System (ECS) |
| **Coral** | C# / .NET 9 CoreCLR managed scripting host |
| **Jolt Physics** | Multithreaded 3D physics simulation |
| **ImGui + ImGuizmo** | Editor user interface and transform gizmos |
| **GLFW + GLAD** | Window creation, input handling, and OpenGL 4.3+ loading |
| **GLM** | Vector and matrix math |
| **Assimp** | 3D model importer |
| **ENet & miniupnpc** | Multiplayer UDP networking and UPnP port forwarding |
| **miniaudio** | Multi-channel 3D audio playback |
| **zstd** | Binary pack compression |
| **spdlog** | Fast logging |

---

## Testing

The project includes both native C++ tests (GoogleTest) and managed C# tests (NUnit):

### 1. C++ Engine Tests (CTest / GoogleTest)
```bash
# Run all unit and integration tests:
ctest --test-dir build/windows-clang -C Debug --output-on-failure

# Run only unit tests:
ctest --test-dir build/windows-clang -C Debug -L Unit --output-on-failure
```

### 2. C# Managed Tests (.NET 9 / NUnit)
```bash
dotnet test "tests/managed/Chained.Managed.Tests.csproj"
```

---

## CI/CD

Continuous Integration runs on every push and PR via GitHub Actions (`.github/workflows/`):
- **Format Check:** Enforces `clang-format` on C++ sources.
- **Windows & Linux Matrix Builds:** Compiles Debug and Release configurations across Clang, MSVC, and GCC.
- **Automated Test Execution:** Runs both GoogleTest suites and C# Managed NUnit tests on all platforms.

---

## Documentation

For in-depth guides and references, check the [`docs/`](docs/) directory:

| Guide | Description |
| :--- | :--- |
| 📖 [User Guide](docs/USER_GUIDE.md) | Getting started, editor workflows, project settings, and building |
| ⚡ [Scripting API Reference](docs/SCRIPTING_API.md) | Complete C# API reference: `[Autoload]`, `[AutoAttach]`, events, input, physics |
| 🛠️ [Scripting Interop Guide](docs/SCRIPTING_INTEROP.md) | Architecture of the C++/C# Coral interop bridge |
| 🏗️ [Engine Architecture](docs/ARCHITECTURE.md) | Core engine initialization, main loop, and subsystem design |
| 🧩 [Component Reference](docs/COMPONENTS.md) | Overview of all built-in ECS components |
| 🎬 [Animation Graphs](docs/ANIMATION_GRAPHS.md) | Tutorial on visual animation graphs and state machines |
| 🐧 [Dual-Platform Guide](docs/DUAL_PLATFORM_GUIDE.md) | Simultaneous Windows + WSL2 development workflow |
| 📦 [Export Guide](docs/EXPORT.md) | Standalone game packaging and distribution |
| ❓ [FAQ](docs/FAQ.md) | Common questions and troubleshooting patterns |

---

## Troubleshooting

- **Coral Status 3 / DotNetNotFound:** Ensure .NET 9.0 SDK is installed (`winget install Microsoft.DotNet.SDK.9`). Coral requires `.NET 9.0.x` hostfxr.
- **Clang not found on Windows:** Add LLVM bin path (`C:\Program Files\LLVM\bin`) to your system `PATH`.
- **Submodules Missing:** Run `git submodule update --init --recursive`.
- **LFS Assets Missing:** Run `git lfs pull` to fetch binary models, skyboxes, and textures.
- **Multiplayer Port Forwarding:** Ensure UDP port 4588 is open in your firewall or UPnP is enabled on your router.

---

## License

This project is licensed under the [MIT License](license).
