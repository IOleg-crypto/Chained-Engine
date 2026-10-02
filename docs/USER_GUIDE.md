# User Guide

Step-by-step guide for getting started with ChainedEngine.

## Table of Contents

- [Requirements](#requirements)
- [Building the Engine](#building-the-engine)
- [Running the Editor](#running-the-editor)
- [Editor Overview](#editor-overview)
- [Creating Your First Scene](#creating-your-first-scene)
- [Writing Your First Script](#writing-your-first-script)
- [Running the Game](#running-the-game)
- [Exporting Your Project](#exporting-your-project)
- [Common Tasks](#common-tasks)

---

## Requirements

| Component | Version |
|---|---|
| C++ Compiler | MSVC 17+, Clang 18+, or GCC 14+ |
| CMake | 3.28+ |
| .NET SDK | 10.0.x (for C# scripting) |
| Git | With submodule support |

**Windows:** Visual Studio 2022 or Clang from LLVM.
**Linux:** `build-essential`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `mesa-common-dev`, `libgl1-mesa-dev`.

## Building the Engine

1. **Clone the repository with submodules:**

```bash
git clone --recurse-submodules https://github.com/IOleg-crypto/Chained-Engine.git
cd Chained-Engine
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

2. **Configure with CMake:**

```bash
# Windows (Clang)
cmake --preset windows-clang-debug

# Windows (MSVC)
cmake --preset windows-msvc-debug

# Linux (Clang)
cmake --preset linux-clang-debug
```

3. **Build:**

```bash
cmake --build --preset windows-clang-debug --parallel
```

4. **Binaries appear in:**

```
build/<preset>/bin/Debug/
  ChainedEditor.exe      # The editor
  ChainedRuntime.exe     # Headless game runner
  ChainedDecos.exe       # Game executable
```

## Running the Editor

```bash
build/windows-clang/bin/Debug/ChainedEditor.exe
```

The editor opens with a default scene. You can open any `.chproject` file through **File > Open Project**.

## Editor Overview

The editor has these main areas:

| Area | Description |
|---|---|
| **Viewport** | 3D preview of the scene. Click to select objects. Gizmos for move/rotate/scale. |
| **Hierarchy** | Tree view of all entities in the scene. Right-click to add/delete. |
| **Inspector** | Properties of the selected entity. Edit components here. |
| **Content Browser** | File browser for assets (models, textures, scripts, scenes). |
| **Console** | Engine logs and errors. |
| **Animation Graph** | Visual state machine editor for animations. |

### Simulation Controls

- **Play** — Runs the game inside the editor. Physics and scripts execute.
- **Simulate** — Runs physics only, no scripts.
- **Stop** — Returns to edit mode.
- **Escape** — Leaves simulation and returns to editor interaction.

## Creating Your First Scene

1. **Create a new scene:** File > New Scene (or Ctrl+N).

2. **Add an entity:** Right-click in the Hierarchy > Create Empty.

3. **Rename it:** Double-click the entity in Hierarchy, type a name (e.g., "Player").

4. **Add a Transform:** The entity already has one by default. Adjust Position/Rotation/Scale in the Inspector.

5. **Add a Model:** In the Inspector, click "Add Component" > ModelComponent. Browse to a `.gltf` or `.obj` file.

6. **Add a Camera:** Create another entity, add CameraComponent. Set it as the main camera.

7. **Add Lighting:** Create an entity, add LightComponent. Choose Point, Spot, or Directional.

8. **Save:** File > Save Scene (Ctrl+S).

### Adding Physics

1. Select your entity.
2. Add **RigidBodyComponent** — choose Static, Dynamic, or Kinematic.
3. Add **ColliderComponent** — choose Box, Sphere, Capsule, or Mesh shape.
4. Press **Play** to see it fall under gravity.

## Writing Your First Script

Scripts are written in C# and live in your game's `assets/scripts/src/` folder.

### 1. Create the script file

Create `assets/scripts/src/MyScript.cs`:

```csharp
using Chained;

namespace MyGame
{
    public class MyScript : Script
    {
        public float Speed = 5.0f;

        public override void OnCreate()
        {
            Log.Info("MyScript created!");
        }

        public override void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyDown(Key.W))
            {
                TransformComponent? transform = GetComponent<TransformComponent>();
                if (transform != null)
                {
                    transform.Translation.Z -= Speed * deltaTime;
                }
            }
        }
    }
}
```

### 2. Attach the script to an entity

1. Select the entity in the Hierarchy.
2. In the Inspector, click "Add Component" > ManagedScriptComponent.
3. Browse to your compiled script (the engine auto-discovers scripts in the assembly).

### 3. Set public fields

Public fields (like `Speed`) appear in the Inspector. You can edit them without recompiling.

### Script Lifecycle

| Method | When it runs |
|---|---|
| `OnCreate()` | Once, when the script is first attached. |
| `OnStart()` | Once, on the first frame after OnCreate. |
| `OnUpdate(float dt)` | Every frame while the game runs. |
| `OnGUI()` | Every frame for in-game UI drawing. |
| `OnCollisionEnter(ulong id)` | When a physics collision starts. |
| `OnDestroy()` | When the script or scene is destroyed. |

### Common Patterns

**Move toward a target:**
```csharp
Vector3 direction = target - transform.Translation;
transform.Translation += Vector3.Normalize(direction) * Speed * deltaTime;
```

**Check collision with a tag:**
```csharp
public override void OnCollisionEnter(ulong otherEntityId)
{
    Entity other = new Entity(otherEntityId);
    TagComponent? tag = other.GetComponent<TagComponent>();
    if (tag?.Tag == "Pickup")
    {
        Log.Info("Collected!");
    }
}
```

**Teleport (use ForceSetVelocity for dynamic bodies):**
```csharp
RigidBodyComponent? rb = GetComponent<RigidBodyComponent>();
rb?.ForceSetVelocity(Vector3.Zero);
transform.Translation = spawnPoint;
```

## Running the Game

### In the Editor

Press **Play** in the toolbar. The game runs inside the viewport. Press **Stop** to return to edit mode.

### With ChainedRuntime

```bash
ChainedRuntime.exe --project path/to/mygame.chproject --width 1920 --height 1080
```

| Flag | Description |
|---|---|
| `--project` | Path to the `.chproject` file. |
| `--name` | Window title (default: project name). |
| `--width` | Window width in pixels. |
| `--height` | Window height in pixels. |

### From Command Line

```bash
ChainedDecos.exe
```

Opens the default project defined in the `.chproject` file.

## Exporting Your Project

1. In the editor, go to **File > Export Project**.
2. Choose a **Pack Mode**:
   - **Fast (LZ4)** — Quick export, larger file.
   - **Balanced (ZSTD)** — Slower export, smaller file.
   - **Raw** — No compression.
3. Adjust **Compression Threshold** if needed (0.0 = compress everything, 1.0 = compress nothing).
4. Click **Browse Output Folder** and select a destination.
5. The export starts automatically. Progress is shown in the overlay.
6. Distribute the exported folder — it contains everything needed to run the game with ChainedRuntime.

## Common Tasks

### Add a Light

1. Create an entity.
2. Add **LightComponent**.
3. Choose type: Point (omnidirectional), Spot (cone), or Directional (sun).
4. Adjust color, intensity, and range in the Inspector.

### Add Audio

1. Create an entity.
2. Add **AudioComponent**.
3. Set the audio file path, volume, pitch.
4. Enable **Spatialized** for 3D positional audio.
5. Enable **PlayOnStart** to play automatically.

### Create a Scene Transition

1. Create an entity with **SceneTransitionComponent**.
2. Set **TargetScenePath** to the destination scene.
3. From a script, set `Triggered = true` when the player reaches the exit.

### Change Window Settings at Runtime

```csharp
AppWindow.SetSize(1920, 1080);
AppWindow.SetFullscreen(true);
AppWindow.SetVSync(false);
```

### Play a Sound from Script

```csharp
Audio.Play("assets/sounds/jump.wav", volume: 0.8f, pitch: 1.0f);
Audio.Stop("assets/sounds/jump.wav");
Audio.StopAll();
```

### Find an Entity by Tag

```csharp
Entity? enemy = Scene.FindEntityByTag("Enemy");
if (enemy != null)
{
    TransformComponent? t = enemy.GetComponent<TransformComponent>();
}
```

### Copy an Entity

```csharp
Entity? clone = Scene.CopyEntity(original);
```

### Switch Scenes from Script

```csharp
Scene.LoadScene("assets/scenes/level2.chscene");
```

### Exit the Game

```csharp
Application.Close();
```

---

For the full C# API reference, see [Scripting API Reference](SCRIPTING_API.md).
For component details, see [Component Reference](COMPONENTS.md).
