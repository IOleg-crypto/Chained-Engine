# Dual-Platform Development Guide (Windows + WSL2)

A comprehensive guide for building, running, and developing ChainedEngine across **Windows** and **Linux (WSL2)** simultaneously using a single shared repository on your Windows drive (e.g. `D:\gitnext\Chained Decos`).

---

## Table of Contents

- [Overview & Architecture](#overview--architecture)
- [WSL2 Setup & NTFS Mount Permissions](#wsl2-setup--ntfs-mount-permissions)
- [Linux Dependencies](#linux-dependencies)
- [Building on Windows](#building-on-windows)
- [Building in WSL2](#building-in-wsl2)
- [Hardware Acceleration (WSLg)](#hardware-acceleration-wslg)
- [Simultaneous Dual-Platform Workflow](#simultaneous-dual-platform-workflow)
- [Cross-Platform Multiplayer Testing](#cross-platform-multiplayer-testing)
- [Troubleshooting](#troubleshooting)

---

## Overview & Architecture

ChainedEngine is designed to be built and run on both Windows and Linux from the exact same project root without file collisions:

```
D:\gitnext\Chained Decos\ (or /mnt/d/gitnext/Chained Decos)
├── build/
│   ├── windows-clang/       <── Native Windows binaries (.exe, .dll)
│   └── linux-clang/         <── Native Linux / WSL2 ELF binaries
├── engine/
│   └── scripting/
│       └── managed/
│           ├── obj/         <── Windows MSBuild intermediate output
│           └── managed-obj/ <── WSL2 / Linux intermediate output (isolated)
```

### Why Dual-Platform Works Seamlessly
1. **Zero Binary Collisions:** Windows outputs to `build/windows-clang/`, while WSL2 outputs to `build/linux-clang/`.
2. **C# Intermediate Isolation:** `Directory.Build.props` automatically redirects Linux MSBuild `obj/` output to `managed-obj/` and excludes stale `obj/` directories via `DefaultItemExcludes`. This prevents `CS0579 Duplicate attribute` errors when switching between operating systems.
3. **Identical Asset Format:** Scenes, GLTF/GLB models, materials, textures, and audio files are platform-agnostic and read directly from `game/chaineddecos/assets`.

---

## WSL2 Setup & NTFS Mount Permissions

By default, WSL2 mounts Windows drives under `/mnt/` using DrvFs without Linux permission metadata. This causes CMake, Ninja, and MSBuild to fail with `Operation not permitted` (`MSB3374`) when updating file timestamps.

### 1. Enable `metadata` in `/etc/wsl.conf`

Inside your WSL2 terminal, run:

```bash
sudo bash -c 'cat << "EOF" > /etc/wsl.conf
[boot]
systemd=true

[user]
default=olegg

[automount]
enabled = true
options = "metadata,umask=22,fmask=11"
mountFsTab = true
EOF'
```

### 2. Apply the Configuration

In Windows PowerShell, restart WSL:
```powershell
wsl --shutdown
```
Or remount immediately inside WSL2:
```bash
sudo mount -o remount,metadata,umask=22,fmask=11 /mnt/d
```

Verify that `metadata` is active:
```bash
mount | grep /mnt/d
# Output should contain: metadata;umask=22;fmask=11
```

---

## Linux Dependencies

Install the required development packages in WSL2 (Ubuntu 24.04 / 22.04):

```bash
# Core build dependencies, X11, OpenGL, Clang 18, and .NET 9 SDK
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build clang lld clang-tools-18 \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libxext-dev libgl1-mesa-dev libglu1-mesa-dev libasound2-dev \
  mesa-utils zlib1g-dev pkg-config python3 dotnet-sdk-9.0

# Ensure clang-scan-deps and dotnet are globally accessible
sudo ln -sf /usr/bin/clang-scan-deps-18 /usr/bin/clang-scan-deps
sudo ln -sf ~/.dotnet/dotnet /usr/local/bin/dotnet 2>/dev/null || true

# Coral CoreCLR discovery requires .NET in /usr/share/dotnet
if [ -d "$HOME/.dotnet" ] && [ ! -d "/usr/share/dotnet" ]; then
  sudo ln -sf "$HOME/.dotnet" /usr/share/dotnet
fi

# Configure ALSA to route to WSLg PulseAudio (fixes "cannot find card 0" and silence)
sudo bash -c 'cat << "EOF" > /etc/asound.conf
pcm.!default {
    type pulse
}
ctl.!default {
    type pulse
}
EOF'
```

---

## Building on Windows

In Windows PowerShell / CMD from the repository root:

```powershell
# Configure (Ninja Multi-Config with Clang)
cmake --preset windows-clang

# Build Debug
cmake --build build/windows-clang --config Debug --parallel

# Run Editor
.\build\windows-clang\bin\Debug\ChainedEditor.exe
```

---

## Building in WSL2

In WSL2 Bash from the repository root:

```bash
cd "/mnt/d/gitnext/Chained Decos"

# Configure directly on Drive D:
cmake --preset linux-clang

# Build all targets in parallel
cmake --build build/linux-clang --config Debug --parallel

# (Optional) Build only the Editor
cmake --build build/linux-clang --config Debug --parallel --target ChainedEditor

# Run Editor
./build/linux-clang/bin/Debug/ChainedEditor
```

---

## Hardware Acceleration (WSLg)

WSLg translates OpenGL 4.3+ calls directly to DirectX 12 via the Mesa D3D12 driver.

To ensure the dedicated GPU (e.g. NVIDIA RTX) is selected rather than the integrated GPU:

Add the following to your `~/.bashrc`:
```bash
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
```

Verify GPU acceleration:
```bash
glxinfo | grep "OpenGL renderer"
# Should display: D3D12 (NVIDIA GeForce RTX ...)
```

---

## Simultaneous Dual-Platform Workflow

Because the build artifacts and C# intermediate directories are strictly separated:

1. **Daily Development & Level Design:**
   - Run the editor natively on Windows for lowest latency, direct WASAPI audio, and full GPU performance.
   - Edit scenes, materials, and C# scripts in Visual Studio / VS Code.
2. **Instant Linux Compatibility Checks:**
   - Keep a WSL2 terminal open alongside your editor.
   - Run `cmake --build build/linux-clang --parallel` at any time to verify that your C++ and C# changes compile cleanly under Linux GCC/Clang.
   - Run unit and integration tests:
     ```bash
     ctest --test-dir build/linux-clang -C Debug --output-on-failure
     ```

---

## Cross-Platform Multiplayer Testing

You can test client-server networking between Windows and Linux locally on a single machine:

1. **Host on Windows:**
   Launch the game or editor in Windows:
   ```powershell
   .\build\windows-clang\bin\Debug\ChainedEditor.exe
   ```
   Start the server in the Network panel on port `7777`.

2. **Client on Linux (WSL2):**
   Launch the Linux binary in WSL2:
   ```bash
   ./build/linux-clang/bin/Debug/ChainedDecos
   ```
   Connect to `127.0.0.1:7777`.

This tests:
- Binary packet serialization across Windows and Linux compilers.
- Endianness and structure alignment.
- ENet threaded transport resilience across different OS network stacks.

---

## Troubleshooting

- **`Operation not permitted` / `MSB3374` during build:**
  Verify `/mnt/d` is mounted with `metadata` by running `mount | grep /mnt/d`. If missing, update `/etc/wsl.conf` and run `wsl --shutdown` in Windows.
- **`ScriptEngine: Failed to initialize Coral! Status: 3`:**
  Coral requires `.NET` in `/usr/share/dotnet`. Run:
  `sudo ln -sf ~/.dotnet /usr/share/dotnet`
- **`Duplicate attribute` / `CS0579` on AssemblyInfo.cs:**
  Ensure `Directory.Build.props` includes `<DefaultItemExcludes>$(DefaultItemExcludes);obj\**;**/obj/**</DefaultItemExcludes>`. Clean old Windows `obj` folders once:
  `rm -rf engine/scripting/managed/obj engine/scripting/managed/*/obj`
- **Mouse camera jumping / spinning in WSLg:**
  In WSLg (XWayland), pointer grabbing (`GLFW_CURSOR_DISABLED`) causes synthetic mouse warping events. ChainedEngine disables raw cursor locking on Linux (`#if !CH_PLATFORM_LINUX`) and clamps delta motion to ensure smooth mouse look.
- **No sound / ALSA `cannot find card '0'` / `Unknown PCM default` in WSL2:**
  WSL2 does not have physical ALSA hardware devices. Install `libasound2-plugins` and configure ALSA to route through WSLg's PulseAudio server:
  ```bash
  sudo bash -c 'cat << "EOF" > /etc/asound.conf
  pcm.!default {
      type pulse
  }
  ctl.!default {
      type pulse
  }
  EOF'
  ```
