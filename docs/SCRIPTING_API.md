# Scripting API Reference (C#)

This document is the reference for the managed C# API available to gameplay scripts
in Chained Engine. Every signature and example here is taken directly from the
sources under `scripting/managed/src/` and is kept in sync with them. When in doubt,
those files are the source of truth.

The managed layer is a thin wrapper over the native engine. C# holds no game state of
its own: each component wrapper forwards to a C++ function pointer bound at startup
through Coral (the .NET/CoreCLR host). A wrapper is therefore only valid while its
entity is valid.

## Contents

0. [Quick reference](#0-quick-reference)
1. [Script lifecycle](#1-script-lifecycle)
2. [Entities and components](#2-entities-and-components)
3. [Components](#3-components)
4. [Input](#4-input)
5. [Scene, application, and services](#5-scene-application-and-services)
6. [Logging](#6-logging)
7. [In-game UI](#7-in-game-ui)
8. [Worked example](#8-worked-example)

---

## 0. Quick reference

| Namespace | Class | Purpose |
|---|---|---|
| `Chained` | `Script` | Base class for all gameplay scripts |
| `Chained` | `Entity` | Wraps a native entity, provides component access |
| `Chained` | `Input` | Keyboard and mouse input queries |
| `Chained` | `Scene` | Find entities, load scenes, copy entities |
| `Chained` | `Audio` | Play/stop sounds |
| `Chained` | `Application` | Application lifecycle (close) |
| `Chained` | `Time` | FPS and delta time |
| `Chained` | `AppWindow` | Window size, fullscreen, vsync, AA |
| `Chained` | `Physics` | Gravity query |
| `Chained` | `UI` | Minimal in-game text rendering |
| `Chained` | `Log` | Console logging (Info, Warn, Error) |

**Key enums** (`Chained` namespace, defined in `Math.cs`):

| Enum | Values |
|---|---|
| `Key` | `A`–`Z`, `D0`–`D9`, `Space`, `Escape`, `Enter`, `Tab`, `LeftShift`, `LeftControl`, `LeftAlt`, arrows, `F1`–`F12`, etc. |
| `MouseButton` | `Left`, `Right`, `Middle` |

---

## 1. Script lifecycle

Every gameplay script derives from `Chained.Script` (`scripting/managed/src/Script.cs`).
The lifecycle methods are `public virtual` — override the ones you need. They are not
`protected`; the engine invokes them across the interop boundary.

```csharp
using Chained;

namespace MyGame
{
    public class Example : Script
    {
        // Called once, one frame after the script is instantiated.
        // Entity is already assigned here.
        public override void OnCreate() { }

        // Called once, on the first Update frame after OnCreate.
        public override void OnStart() { }

        // Called every frame while the simulation runs.
        public override void OnUpdate(float deltaTime) { }

        // Called during the UI pass. Use the UI helper here (see section 7).
        public override void OnGUI() { }

        // Called when the physics system reports a contact. The argument is the
        // other entity's raw id, not an Entity wrapper — construct one if needed.
        public override void OnCollisionEnter(ulong otherEntityId) { }

        // Called when a native engine event is forwarded. eventType is the
        // integer value of the native EventType enum.
        public override void OnEvent(int eventType) { }

        // Called once when the script is torn down.
        public override void OnDestroy() { }
    }
}
```

Ordering guarantees: `OnCreate` runs one frame after instantiation, `OnStart` runs the
frame after `OnCreate`, and `OnUpdate` only begins once `OnStart` has completed. This
staging is deliberate — it prevents `OnUpdate` from running on the same frame the
script was created.

### Accessing the owning entity

The base class exposes the entity the script is attached to, plus shortcuts:

```csharp
public Entity Entity { get; }              // the owning entity
public T? GetComponent<T>() where T : Component, new();   // shortcut to Entity.GetComponent
public bool HasComponent<T>() where T : Component, new(); // shortcut to Entity.HasComponent
```

---

## 2. Entities and components

`Entity` (`scripting/managed/src/Entity.cs`) wraps a native entity id and provides
component access. Component lookups are cached per entity, so repeated
`GetComponent<T>()` calls do not re-allocate.

```csharp
public ulong ID { get; }
public bool IsValid { get; }                       // ID != 0
public TransformComponent? Transform { get; }      // shortcut, null if absent

public bool HasComponent<T>() where T : Component, new();
public T? GetComponent<T>() where T : Component, new();   // null if the entity lacks T
public T AddComponent<T>() where T : Component, new();
public static ulong[] FindAllWithComponent<T>() where T : Component, new();
public void InvalidateComponentCache();  // clears cached component wrappers
```

`GetComponent<T>()` returns `null` when the component is absent, so always null-check
before use:

```csharp
public override void OnUpdate(float deltaTime)
{
    RigidBodyComponent? rb = GetComponent<RigidBodyComponent>();
    if (rb == null)
        return;

    rb.Velocity = new Vector3(0.0f, 0.0f, -5.0f);
}
```

> Note: the C# type name is what the native side matches against. `GetComponent<T>()`
> sends `typeof(T).Name` (for example `"RigidBodyComponent"`) to the engine, which
> resolves it against the component registry. If a lookup unexpectedly returns null,
> confirm the component is actually present on the entity.

---

## 3. Components

Component wrappers live in `scripting/managed/src/Components/`. All derive from
`Component`, which itself exposes `Entity` and a `Transform` shortcut.

> 💡 **Native Binding Architecture**: Components use Roslyn Source Generation (`[NativeProperty]` and `[NativeCall]` attributes). Function pointers (`_Ptr` fields) and C# getters/setters are auto-generated at compile time. See [SCRIPTING_INTEROP.md](SCRIPTING_INTEROP.md) for details on extending components and adding native calls.

The wrappers available to scripts are listed below with their public members.

### TransformComponent

```csharp
public Vector3 Translation { get; set; }
public Vector3 Rotation    { get; set; }   // Euler angles (radians)
public Vector3 Scale       { get; set; }
```

### RigidBodyComponent

```csharp
public Vector3 Velocity    { get; set; }
public bool    IsGrounded  { get; }        // read-only, driven by the physics world
public bool    IsKinematic { get; set; }
public float   Mass        { get; set; }
public void    ForceSetVelocity(Vector3 velocity);  // bypasses Jolt's internal override
```

### CameraComponent

```csharp
public Vector3 Forward { get; }            // derived from the entity transform
public Vector3 Right   { get; }
public bool    Primary { get; set; }
public bool    IsOrbitCamera { get; set; }
public string  TargetEntityTag { get; set; }

public void GetOrbit(out float yaw, out float pitch, out float distance);
public void SetOrbit(float yaw, float pitch, float distance);
```

### Other wrappers

| Wrapper | Key members |
| :--- | :--- |
| `ModelComponent` | `string ModelPath { get; set; }` |
| `TagComponent` | `string Tag { get; }` |
| `AudioComponent` | `float Volume { set; }`, `bool Loop { set; }`, `bool IsPlaying { get; }`, `string SoundPath { get; }`, `Play()`, `Stop()` |
| `SpriteComponent` | `string TexturePath { get; set; }`, `Vector4 Tint { get; set; }`, `bool FlipX { get; set; }`, `bool FlipY { get; set; }`, `int ZOrder { get; set; }` |
| `ShaderComponent` | `bool Enabled { get; set; }`, `SetFloat(name, value)`, `SetVector3(name, value)` |
| `PlayerComponent` | `float MovementSpeed { get; set; }`, `float JumpForce { get; set; }`, `float LookSensitivity { get; set; }` |
| `SpawnComponent` | `bool IsActive { get; set; }`, `bool IsCheckpoint { get; set; }`, `Vector3 SpawnPoint { get; set; }`, `bool RenderSpawnZoneInScene { get; }`, `Vector3 ZoneSize { get; }` |
| `AnimationComponent` | `int CurrentAnimationIndex { get; set; }`, `bool IsPlaying { get; set; }`, `bool IsLooping { get; set; }`, `bool IsFinished { get; }`, `float Duration { get; }`, `float NormalizedTime { get; }`, `float BlendDuration { get; set; }`, `Play()`, `Pause()`, `CrossFade(int index, float duration)`, `SetFloat(name, value)`, `SetBool(name, value)`, `GetFloat(name)` |

---

## 4. Input

`Input` (`scripting/managed/src/Input.cs`) is a static class. Keyboard queries take the
`Key` enum; mouse-button queries take the `MouseButton` enum (both in
`scripting/managed/src/Math.cs`). Note the enum is `Key`, not `KeyCode`.

```csharp
public static bool  IsKeyDown(Key key);          // held this frame
public static bool  IsKeyPressed(Key key);       // went down this frame
public static bool  IsKeyReleased(Key key);      // went up this frame
public static bool  IsMouseButtonDown(MouseButton button);
public static bool  IsMouseButtonPressed(MouseButton button);
public static float GetMouseWheelMove();         // scroll delta this frame
public static Vector3 MouseDelta { get; }        // (dx, dy, 0) since last frame
```

`Key` covers `A`–`Z`, `Space`, `Escape`, `Enter`, `Tab`, arrows, function keys,
modifiers such as `LeftShift`/`LeftControl`, and the digit keys `D0`–`D9`.
`MouseButton` is `Left`, `Right`, or `Middle`.

```csharp
public override void OnUpdate(float deltaTime)
{
    if (Input.IsKeyDown(Key.W))
    {
        // move forward
    }

    if (Input.IsMouseButtonDown(MouseButton.Right))
    {
        Vector3 delta = Input.MouseDelta;   // look around
    }
}
```

---

## 5. Scene, application, and services

These static classes live in `scripting/managed/src/SceneAndApplication.cs`.

### Scene

```csharp
public static Entity? FindEntityByTag(string tag);   // null if not found
public static void    LoadScene(string path);        // e.g. "scenes/level1.chscene"
public static Entity? GetMainCamera();
public static Entity? CopyEntity(Entity entity);      // null on failure
```

### Audio

```csharp
public static void Play(string path, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
public static void Stop(string path);
public static void StopAll();
```

### Application

```csharp
public static void Close();   // request application shutdown
```

### Time

```csharp
public static int   FPS { get; }
public static float DeltaTime { get; }
```

### Physics

```csharp
public static float GetGravity();   // world gravity from project settings
```

### AppWindow

```csharp
public static void   SetSize(int width, int height);
public static void   SetFullscreen(bool enabled);
public static void   SetVSync(bool enabled);
public static void   SetAntialiasing(bool enabled);
public static void   SetAntiAliasingSamples(int samples);
public static string GetSupportedResolutions();
```

---

## 6. Logging

`Log` (`scripting/managed/src/Log.cs`) writes to the same buffered console the editor
displays. Messages are plain strings — format them yourself with interpolation.

```csharp
public static void Info(string message);
public static void Warn(string message);
public static void Error(string message);
public static void ClearHistory();
public static IReadOnlyList<string> History { get; }
```

```csharp
Log.Info($"Player spawned at {transform.Translation}");
Log.Warn("No camera tagged 'Main' in scene");
Log.Error("Failed to load save file");
```

---

## 7. In-game UI

The managed UI surface (`scripting/managed/src/UI.cs`) is intentionally minimal today.
It exposes a single call, used from `OnGUI`:

```csharp
public static void Text(string text);
```

```csharp
public override void OnGUI()
{
    UI.Text($"Score: {_score}");
    UI.Text($"FPS: {Time.FPS}");
}
```

For richer player-facing UI, prefer the native declarative path (`WidgetComponent` +
`SceneTransitionComponent`) described in the README. `UI.Text` is meant for lightweight
HUD readouts, not full menus.

---

## 8. Worked example

A camera-relative movement controller that reads WASD, moves a rigid body, and jumps.
This mirrors the shape of the real `PlayerController` in
`game/chaineddecos/assets/scripts/src/`.

```csharp
using Chained;

namespace MyGame
{
    public class Mover : Script
    {
        public float Speed = 15.0f;
        public float JumpForce = 15.0f;

        public override void OnCreate()
        {
            Log.Info("Mover ready");
        }

        public override void OnUpdate(float deltaTime)
        {
            // Camera-relative ground directions.
            Vector3 forward = Vector3.Zero;
            Vector3 right = Vector3.Zero;

            Entity? camEntity = Scene.GetMainCamera();
            CameraComponent? camera = camEntity?.GetComponent<CameraComponent>();
            if (camera != null)
            {
                forward = Vector3.Normalize(new Vector3(camera.Forward.X, 0.0f, camera.Forward.Z));
                right   = Vector3.Normalize(new Vector3(camera.Right.X,   0.0f, camera.Right.Z));
            }

            Vector3 dir = Vector3.Zero;
            if (Input.IsKeyDown(Key.W)) dir += forward;
            if (Input.IsKeyDown(Key.S)) dir -= forward;
            if (Input.IsKeyDown(Key.A)) dir -= right;
            if (Input.IsKeyDown(Key.D)) dir += right;

            RigidBodyComponent? rb = GetComponent<RigidBodyComponent>();
            if (rb == null)
                return;

            Vector3 velocity = rb.Velocity;

            if (dir.LengthSquared() > 0.0001f)
            {
                dir = Vector3.Normalize(dir);
                velocity.X = dir.X * Speed;
                velocity.Z = dir.Z * Speed;
            }
            else
            {
                velocity.X = 0.0f;
                velocity.Z = 0.0f;
            }

            // Jump: physics owns the vertical axis for dynamic bodies.
            if (Input.IsKeyPressed(Key.Space) && rb.IsGrounded)
                velocity.Y = JumpForce;

            rb.Velocity = velocity;
        }

        public override void OnCollisionEnter(ulong otherEntityId)
        {
            Entity other = new Entity(otherEntityId);
            TagComponent? tag = other.GetComponent<TagComponent>();
            if (tag != null && tag.Tag == "Hazard")
                Log.Info("Hit a hazard");
        }
    }
}
```
