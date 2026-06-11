# Chained Decos 

### _A High-Performance 3D Parkour Engine & Game Powered by Chained Engine_

[![C++23](https://img.shields.io/badge/language-C%2B%2B23-blue?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/IOleg-crypto/Chained-Decos/build.yml)](https://github.com/IOleg-crypto/Chained-Decos/actions)
[![raylib](https://img.shields.io/badge/raylib-5.5--dev-red?logo=raylib)](https://www.raylib.com/)


> [!WARNING]
> **Deprecated Branch**: This branch is old and **no longer supported**. The engine is currently being completely rewritten and migrated to a new modern architecture using pure OpenGL. Please check other active branches for the latest development.

**Chained Decos** is a 3D parkour game built from the ground up using **Chained Engine**, a custom modular C++23 game engine. It features advanced physics, an ECS-driven architecture, native C++ scripting, and integrated development tools.

![Game Screenshot](https://i.imgur.com/d9Bxmsq.jpeg)

---

## The Chained Engine Architecture

Chained Engine follows a modern, modular design inspired by professional game engines:

- **Entity Component System (ECS)**: Powered by [EnTT](https://github.com/skypjack/entt) for high-performance entity management.
- **Native C++ Scripting**: Custom scripting DSL for gameplay logic with hot-reloading support.
- **Virtual File System**: Unified asset access with support for engine-relative paths and project-relative assets.(Planned)
- **Advanced Physics**: BVH-accelerated collision detection with robust world-to-local coordinate transformations.
- **Project Hub**: Integrated project browser with recent project tracking and intelligent path resolution.
- **Cross-Platform Core**: Designed for portability; current support for Windows and Linux, with an architecture that allows for easy integration of **new platforms**.

## Editor & Simulation Workflow

The Chained Editor provides a high-fidelity environment for creating and testing parkour courses.

> [!IMPORTANT]
> **Simulation Mode Controls**:
>
> - **Capture**: When you press **PLAY**, the editor captures the cursor for game control.
> - **ESCAPE**: No longer exits the app! Press **ESCAPE** during play to return to editor control.

![Editor Screenshot#1](https://i.postimg.cc/CMrw4RW5/Znimok-ekrana-2026-01-18-115501.png)
![Editor Screenshot#2](https://i.postimg.cc/1XhrxmdQ/Znimok-ekrana-2026-01-18-122026.png)
![Editor Screenshot#3](https://i.postimg.cc/ZK3tPGXk/Znimok-ekrana-2026-02-01-125758.png)

_GUI Development is currently in progress and may be unstable. It is recommended to use the **ChainedEditor** for GUI development._

[![Editor Screenshot#4](https://i.postimg.cc/dtRNVwzr/Znimok-ekrana-2026-02-01-133334.png)](https://postimg.cc/4mdQ8k4x)

> [!WARNING]
> **Standalone Runtime**: The `ChainedRuntime` acts as a specialized **wrapper** for your games. It is designed to load and execute your custom projects directly. While it now supports Linux and Windows, it is still under active development.

---

### Engine Features

- **High-Performance Rendering**: Optimized Raylib-based renderer with custom shader support and PBR materials.
- **Native C++ Scripting**: Type-safe gameplay scripts with DSL macros for clean, maintainable code.
- **Advanced Physics & Diagnostics**: BVH-accelerated collision detection with real-time logging for collider state.
- **Project Hub**: Split-view launcher with "Recent Projects" and intelligent project folder creation.
- **Undo/Redo System**: Full command history for editor operations (Add/Delete/Move/Transform).
- **Scene Serialization**: YAML-based format with UUID tracking and hierarchy preservation.
- **Deduplicated Loading**: Optimized asset pipeline that prevents redundant asset tasks and improves load times.

---

## Installation & Build

### Prerequisites

| Tool | Version | Notes |
| :--- | :--- | :--- |
| **CMake** | 3.25+ | Required for build configuration. |
| **Ninja** | Latest | Highly recommended for fast, parallel builds. |
| **Compiler** | C++23 | Clang 18+, GCC 14+, or MSVC 2022 (17.6+). |
| **Vulkan/GL** | - | OpenGL 4.3+ compatible drivers. |

#### Linux Dependencies
On Ubuntu/Debian, install the required development libraries:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
    libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libasound2-dev libglu1-mesa-dev \
    pkg-config
```

### Build Using CMake Presets

The project uses [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) to simplify cross-platform configuration.

#### 1. Configuration
Choose a preset based on your OS and preferred compiler:

| OS | Compiler | Generator | Preset Name |
| :--- | :--- | :--- | :--- |
| **Linux** | **Clang** | Ninja | `linux-clang` |
| **Linux** | **GCC** | Ninja | `linux-gcc` |
| **Windows** | **MSVC** | VS 2022 | `windows-vs2022` |
| **Windows** | **Ninja** | Ninja | `windows-ninja` |

```bash
# Example: Configure for Release on Windows
cmake --preset windows-ninja -DCMAKE_BUILD_TYPE=Release
```

> [!CAUTION]
> **Switching Presets**: When switching between different generators (e.g., from `windows-ninja` to `windows-vs2022`), you **must delete the `CMakeCache.txt`** in the corresponding build directory or delete the entire build folder for a clean slate.

#### 2. Compilation
Compile all targets (Editor, Runtime, Tests) using a build preset:

```bash
# Example: Build the current configuration
cmake --build --preset windows-ninja -j 8
```

### Launching the Application

Binaries are located in `build/<preset-name>/bin/`.

#### Running the Editor
The **Chained Editor** is the primary tool for course creation and live simulation.
```bash
# Linux
./build/linux-clang/bin/ChainedEditor

# Windows
.\build\windows-ninja\bin\ChainedEditor.exe
```

#### Running the Standalone Runtime
The **Chained Runtime** is a lightweight wrapper that loads and runs your own projects/games without the editor's UI overhead. By passing your project file as an argument, you can test the final "player experience" of your game.
```bash
# Linux
./build/linux-clang/bin/ChainedRuntime path/to/your.chproject

# Windows
.\build\windows-ninja\bin\ChainedRuntime.exe path\to\your.chproject
```

---

## Testing & CI

We use **Google Test** for engine and scene validation.

### Running Tests Locally
After building, you can run the test suite:
```bash
# Run all tests via CTest
ctest --preset linux-clang

# Or run individual test binaries
./build/linux-clang/bin/tests/EngineTests
```

---

## Contributing & Community

Contributions are welcome! As a project in active development, especially regarding the newly added **Linux support**, you may encounter bugs or platform-specific issues.

### How to Help
- **Bug Reports**: Open an issue describing the problem.
- **Pull Requests**: We highly encourage you to help maintain and improve the engine by creating branches and submitting **Pull Requests**.
- **Platform Support**: If you are a Linux developer, your expertise in refining the build process and runtime compatibility is greatly appreciated.

This project is licensed under the **MIT License**.

Made with using **Raylib**, **ImGui**, and **Jolt** (Modern C++23).
