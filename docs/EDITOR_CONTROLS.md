# Editor Controls

Complete reference for viewport navigation, selection, and editor shortcuts —
including a trackpad-friendly scheme for laptops without a middle mouse button.

## Table of Contents

- [Viewport Navigation](#viewport-navigation)
- [Trackpad Controls](#trackpad-controls)
- [Selection & Gizmos](#selection--gizmos)
- [Keyboard Shortcuts](#keyboard-shortcuts)
- [Playback](#playback)

---

## Viewport Navigation

Hover the mouse over the viewport for camera input to be active.

| Input | Action |
|---|---|
| **RMB drag** | Free-look rotate + WASD/QE fly (cursor captured while held) |
| **RMB + Shift** | Fly speed boost |
| **MMB drag** | Rotate |
| **Shift + MMB drag** | Pan |
| **Alt + LMB drag** | Orbit around the focal point |
| **Scroll wheel** | Zoom toward/away from the focal point |
| **W / A / S / D** | Fly forward / left / back / right (while RMB held) |
| **Q / E** | Fly down / up (while RMB held) |

Zoom can be disabled entirely via **Editor Settings → Disable Camera Zoom**.
Speeds are configurable in **Editor Settings** (move speed, boost multiplier,
rotation speed, zoom speed multiplier).

## Trackpad Controls

Laptops with a precision touchpad have no middle button — these bindings make
full navigation possible with two-finger gestures and modifier keys only.

| Input | Action |
|---|---|
| **Two-finger scroll (vertical)** | Zoom (same as wheel) |
| **Ctrl + two-finger scroll** | Precision zoom — smaller, finer steps |
| **Two-finger scroll (horizontal)** | Pan horizontally |
| **Shift + two-finger scroll** | Pan (both axes) — replaces Shift+MMB |
| **Shift + LMB drag** | Pan (map style) — replaces MMB pan |
| **Alt + LMB drag** | Orbit — works the same as with a mouse |
| **Two-finger click (RMB) drag** | Free-look + WASD fly |

Notes:

- Shift + scroll pan uses the same pan speed curve as the MMB pan, scaled so one
  scroll notch feels like a ~100 px drag.
- Alt + LMB orbit takes priority over Shift + LMB pan when both are held.
- Horizontal scroll always pans even without Shift; vertical scroll zooms.

## Selection & Gizmos

| Input | Action |
|---|---|
| **LMB click** | Select entity under cursor |
| **Gizmo handles** | Drag to move / rotate / scale the selection |
| **Q / W / E / R** | Switch tool: None / Translate / Rotate / Scale |
| **Ctrl + D** | Duplicate selected entity |
| **Delete** (Hierarchy) | Delete selected entity |

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| **Ctrl + Z** | Undo |
| **Ctrl + Y** / **Ctrl + Shift + Z** | Redo |
| **Ctrl + S** | Save scene |
| **Ctrl + Shift + S** | Save scene as |
| **Ctrl + N** | New scene |
| **Ctrl + O** | Open scene |
| **Ctrl + R** | Reload scripts |
| **F11** | Toggle fullscreen |
| **Escape** | Exit fullscreen / leave simulation |

Undo/redo are also available from the **Edit** menu.

## Playback

| Input | Action |
|---|---|
| **Play** | Run the game in the viewport (physics + scripts) |
| **Simulate** | Run physics only |
| **Stop** | Return to edit mode |
| **Escape** | Leave simulation |
