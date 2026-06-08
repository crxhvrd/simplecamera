# Simple Camera — Free Cam & Cinematic Photo Mode for GTA V

A lightweight but deep free-camera and photo-mode plugin for Grand Theft Auto V,
built on ScriptHookV. Detach the camera from your character and fly anywhere in
6 degrees of freedom, then dial in the shot with depth of field, procedural
camera shake, time-of-day and weather control, slow motion, and a world freeze.

When you want motion instead of a still, switch to **Camera Sequence** mode and
author a keyframed camera move with smooth spline paths, easing, and a timeline
of effect automation — then play it back live or render it out to a lossless
image sequence for video editing.

Works in **Story Mode** and is **FiveM-aware** (time/weather use the correct
network natives when running under FiveM).

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Controls](#controls)
- [The Menu](#the-menu)
  - [Free Camera](#free-camera-menu)
  - [Camera Sequence](#camera-sequence-menu)
- [Cinematic Sequences — Tutorial](#cinematic-sequences--tutorial)
- [Rendering an Image Sequence](#rendering-an-image-sequence)
- [ReShade / IGCS Connector](#reshade--igcs-connector)
- [Configuration File](#configuration-file)
- [Building from Source](#building-from-source)
- [Troubleshooting](#troubleshooting)
- [Credits & License](#credits--license)

---

## Features

**Free Camera**

- True 6-DOF free flight with smooth keyboard, mouse, and controller input.
- **Two rotation engines:** standard Euler (gimbal-limited, stays level) or a
  **Quaternion "Acrobatic"** engine for unlimited pitch/roll with no gimbal lock.
- **Drone mode** — momentum-based flight with drag, acceleration, gravity, and
  automatic banking into turns for a natural FPV-drone feel.
- **Walk mode** — terrain-follows the ground at a fixed eye height.
- **World collision** so the camera doesn't clip through geometry (optional).
- **Lock altitude** to keep a constant height while you fly horizontally.
- **Speed boost** (Shift) and **precision/slow** (Alt) movement modifiers.
- **Follow / Entity Lock** — make the camera ride along with the player or any
  aimed-at vehicle/ped, optionally tracking its rotation (rigid mode).
- Adjustable speed, look sensitivity, zoom speed, roll speed, and FOV.

**Photo / Scene control**

- **Depth of Field** with manual focus distance or raycast **auto-focus**, plus
  near/far focus-range control.
- **Procedural camera shake** — presets (Subtle, Handheld, Vehicle, Earthquake)
  or fully custom amplitude/frequency, with speed-coupling so the shake reacts
  to how fast the camera is moving.
- **Time of day** control + pause, with a **time-lapse** mode.
- **Weather** selection and two-weather **blending**.
- **Pause Game**, **Freeze All Entities** (camera stays live), and **Slow
  Motion** for the world while the camera flies at full speed.
- **Hide HUD**, **hide the player character**, on-screen info overlay.

**Camera Sequence (cinematics)**

- Keyframe the camera over a timeline (position, rotation, FOV).
- Per-keyframe **easing** (Linear, Ease-In, Ease-Out, Ease-In-Out, Hold) and
  **path type** (straight Linear or smooth **Catmull-Rom spline**).
- **Effect-event track** — schedule shake and world-speed changes along the
  timeline, with optional ramping between events.
- **Entity lock** — anchor keyframes to a moving vehicle/ped so the shot rides
  along lag-free.
- **Loop closure** for perfectly seamless looping moves.
- **Render to an image sequence** (PNG/lossless or JPEG) with optional
  multi-sample **motion blur**, for turning a sequence into a video.

**Quality of life**

- All tunables persist to a plain `SimpleCamera.ini`.
- Sequences persist to `SimpleCamera_Sequences.ini`.
- Full **controller** support (no keyboard needed to drive the menu or camera).

---

## Requirements

- **Grand Theft Auto V** (the build supported by your ScriptHookV — i.e. the
  classic/legacy version of the game).
- **[ScriptHookV](http://www.dev-c.com/gtav/scripthookv/)** by Alexander Blade.
- *(Optional, only for rendering / advanced photo tools)* **ReShade with add-on
  support** and the **IgcsConnector** add-on.

---

## Installation

1. Install **ScriptHookV** if you don't already have it. This places
   `dinput8.dll` and `ScriptHookV.dll` in your GTA V folder (the folder that
   contains `GTA5.exe`).
2. Copy **`SimpleCamera.asi`** into that same GTA V root folder.
3. *(Optional)* Copy **`SimpleCamera.ini`** alongside it if you want to change
   the menu key or pre-set defaults. If it's missing, the mod just uses built-in
   defaults and writes the file when you save.
4. Launch the game. Press **F5** to open the menu.

> The `.ini`, `SimpleCamera_Sequences.ini`, and any rendered frames
> (`SimpleCamera_Captures/`) are all created next to the `.asi`.

---

## Quick Start

1. Press **F5**. The first time, you'll see a **mode picker** — choose
   **Free Camera**.
2. The flycam engages immediately (HUD and your character auto-hide).
3. Fly with **WASD**, look with the **mouse**, raise/lower with **Space / Ctrl**.
4. Press **F5** again any time to open the settings menu and tweak the shot.
5. Close the menu and you're composing. Press **F10** to save a clean frame
   (requires the ReShade add-on — see below), or use your normal screenshot key.
6. To leave the flycam, open the menu and choose **Exit** (returns to the
   picker).

---

## Controls

### Free Camera — Keyboard & Mouse

| Action | Key |
| --- | --- |
| Open / close menu | **F5** |
| Move forward / left / back / right | **W / A / S / D** |
| Move up / down | **Space / Ctrl** |
| Look | **Mouse** |
| Roll left / right | **Q / E** |
| Zoom in / out (FOV) | **+ / -** (also numpad +/-) |
| Adjust fly speed | **Mouse wheel up / down** |
| Speed boost (turbo) | **Hold Shift** |
| Precision / slow | **Hold Alt** |

### Free Camera — Controller

| Action | Button |
| --- | --- |
| Open menu | **LB + RB** |
| Exit flycam to picker | **LB + B** |
| Move | **Left stick** |
| Look | **Right stick** |
| Up / down | **RT / LT** |
| Roll | **LB / RB** |
| Zoom (FOV) | **D-Pad Up / Down** |

### Menu navigation

| Action | Keyboard | Controller |
| --- | --- | --- |
| Move / adjust | **Arrow keys** | **D-Pad** |
| Select / toggle | **Enter** | **A** |
| Back | **Backspace** | **B** |

### Camera Sequence hotkeys (in Sequence mode)

| Action | Key |
| --- | --- |
| Capture current pose as a keyframe | **F6** |
| Play | **F7** |
| Stop | **F8** |
| Jump to next pose | **F9** |

> The menu key (F5) and the four sequence hotkeys are all configurable in the
> INI.

---

## The Menu

Press **F5** to open. The first F5 of a session shows the **mode picker**
(Free Camera vs. Camera Sequence); after that, F5 reopens the menu for whichever
mode you're in. Choosing **Exit** in either mode returns you to the picker so you
can switch.

### Free Camera menu

| Submenu | What's inside |
| --- | --- |
| **Movement** | Camera speed, look sensitivity, zoom speed, roll speed, **World Collision**, **Lock Altitude**, **Walk Mode** (+ height), **Rotation Style** (Euler / Quaternion "Acrobatic"), **Follow Target** (None / Player / Aimed Entity, with rigid-mode + marker), **Movement Style** (Standard / **Drone** with drag, acceleration, gravity, banking, and smoothing). |
| **Lens settings** | Field of view (zoom) and roll (lens tilt). |
| **Depth of field** | Enable, **Auto-Focus**, manual focus distance, near & far focus range. |
| **Camera effects** | Procedural shake: enable, **preset** (Off / Subtle / Handheld / Vehicle / Earthquake / Custom), base amplitude & frequency, speed→amplitude and speed→frequency coupling, rotation/position weighting, **Stop When Still**, **Randomize Pattern**. |
| **World & scene** | **Time & Weather** (pause time, time of day, time-lapse, primary/secondary weather + blend), **Pause Game**, **Freeze All Entities**, **Slow Motion**, hide HUD, hide player, disable vehicle shake, info overlay. |
| **Misc settings** | Move player with camera, save position on exit, lock camera position, allow player to move, snap camera to player, level horizon, save settings to INI, reset to defaults. |
| **Exit** | Tear down the flycam and return to the mode picker. |

### Camera Sequence menu

| Row | What it does |
| --- | --- |
| **Sequence / New** | Create and cycle between named sequences. |
| **Play / Pause**, **Stop** | Drive playback. |
| **Loop** | Off / On — shows whether the loop seam is *closed* (seamless) or *open* (jumpy). |
| **Speed** | Playback speed multiplier. |
| **Time** | Scrub to a position on the timeline. |
| **Pose Keyframes…** | List/add/edit/delete keyframes; per-pose easing & path type. |
| **Effect Events…** | Schedule shake / world-speed changes along the timeline. |
| **Capture Current Pose** | Snapshot the live camera into a new keyframe (**F6**). |
| **Close Loop** | Append/snap a closing keyframe for a seamless loop (shows the gap). |
| **Follow & Entity Lock…** | Bulk-anchor keyframes to a moving entity, or clear locks. |
| **Show Markers** | Toggle the in-world keyframe spheres + path preview (turn off when recording). |
| **World & Scene…** | Same scene controls as Free Camera. |
| **Render to Images…** | Offline image-sequence renderer (see below). |
| **Exit** | Return to the mode picker. |

---

## Cinematic Sequences — Tutorial

1. Press **F5** → pick **Camera Sequence**. An empty sequence is created and you
   can free-fly the camera while the menu is open.
2. Fly to your first shot. Press **F6** (or **Capture Current Pose**) to drop a
   keyframe.
3. Fly to the next position/angle/zoom and press **F6** again. Repeat for as many
   keyframes as you want.
4. Open **Pose Keyframes…** to fine-tune any keyframe's time, and set its
   **easing** (try **Ease-In-Out** for smooth starts/stops) and **path type**
   (**Spline** for flowing curves through your points).
5. *(Optional)* Open **Effect Events…** to add a shake or slow-motion change at a
   specific time on the timeline.
6. *(Optional)* To ride along with a moving car: in Free Camera, lock onto the
   vehicle (Follow Target → Aimed Entity), then use **Follow & Entity Lock…** to
   anchor your keyframes to it.
7. Press **F7** to play, **F8** to stop. Adjust **Speed** and **Loop** to taste.
   Use **Close Loop** if you want a perfectly seamless repeating move.
8. Turn off **Show Markers** before recording so the helper spheres don't appear
   in your footage. Press **F8** when you're done.
9. **Save All to INI** keeps your sequences between sessions.

---

## Rendering an Image Sequence

Camera Sequence mode can render your move out to a numbered folder of image
files — ideal for assembling a clean, stutter-free clip in a video editor.

1. **Requires ReShade (with add-on support) + the IgcsConnector add-on** (see
   next section). The menu shows a warning if the add-on isn't detected.
2. In the Sequence menu open **Render to Images…** and set:
   - **Output FPS**, **Settle/Flush Frames** (let the frame stabilise before
     capture).
   - **Motion Blur** samples, **Shutter**, **Blur Strength**, **Highlight
     Boost** for cinematic blur.
   - **Format** (PNG lossless or JPEG + quality) and optional **World Slow-mo**.
3. Choose **Start Render**. Frames are written to
   `SimpleCamera_Captures/render_NNNN/` next to the `.asi`.
4. Import the image sequence into your editor at the same FPS.

> Press **F10** at any time in Free Camera for a quick single-frame capture test
> (also via the ReShade add-on).

---

## ReShade / IGCS Connector

Simple Camera implements the **IGCS Connector** protocol, so ReShade's
IgcsConnector add-on can read the camera's live position/rotation/FOV and drive
its advanced photo features (e.g. multi-shot / high-resolution captures and DoF
sessions). When such a session is active, Simple Camera hands control of the
camera to the add-on so the two never fight.

The same shared channel is what powers **F10 single-frame capture** and the
**image-sequence renderer**. None of the core free-camera features need ReShade —
it's only required for the capture/render and IGCS tooling.

---

## Configuration File

`SimpleCamera.ini` (next to the `.asi`) holds the menu key and all saved
tunables. Key codes use Windows
[virtual-key codes](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes).

```ini
[Controls]
; Menu toggle key (default: F5 = 116)
MenuKey=116
; Camera Sequence hotkeys
SequenceAddKey=117   ; F6 — capture pose
SequencePlayKey=118  ; F7 — play
SequenceStopKey=119  ; F8 — stop
SequenceNextKey=120  ; F9 — next pose
```

Camera, Drone, Shake, DoF, and Misc settings are written under their own
sections when you choose **Save Settings to INI** in the Misc menu. **Reset to
Defaults** restores the factory values (save afterward to persist).

---

## Building from Source

- **Visual Studio 2022** (toolset **v143**), **x64 / Release**.
- The ScriptHookV SDK (headers + `ScriptHookV.lib`) is expected under
  `external/scripthook_sdk/`.
- Open `FreeCameraPlugin.sln` and build. Output is **`SimpleCamera.asi`** in
  `bin/Release/`.

Source layout:

| File | Responsibility |
| --- | --- |
| `main.cpp` | DLL entry; registers the script + keyboard handler with ScriptHookV. |
| `script.cpp` | Main per-frame loop and mode dispatch. |
| `camera.cpp` / `.h` | The 6-DOF camera, rotation engines, drone physics, shake, DoF, time/weather, follow/lock. |
| `menu.cpp` / `.h` | The in-game menu, INI load/save, input. |
| `sequence.cpp` / `.h` | Keyframe sequences, easing/splines, effect events, persistence. |
| `fx_capture.cpp` / `.h` | Shared-memory bridge to the ReShade capture add-on. |
| `igcs_bridge.cpp` / `.h` | IGCS Connector protocol implementation. |
| `keyboard.cpp` / `.h` | Async keyboard state tracking. |

---

## Troubleshooting

- **Menu won't open / nothing happens on F5** — Make sure ScriptHookV is
  installed and up to date for your game version, and that `SimpleCamera.asi` is
  in the GTA V root folder. Confirm `MenuKey` in the INI isn't bound to
  something else.
- **"Needs ReShade + IGCS addon" on Start Render** — The renderer and F10
  capture need ReShade (add-on support) plus the IgcsConnector add-on loaded.
  Core free-cam features work without them.
- **Camera clips through walls** — Enable **World Collision** in the Movement
  menu (it's off by default so flight stays unobstructed).
- **Time/weather changes don't stick in FiveM** — The mod auto-detects FiveM and
  uses the network override natives; if a server actively re-syncs time/weather,
  it may override the mod.
- **Acrobatic (Quaternion) rotation needs a model to stream in** — On the very
  first frames it briefly falls back to standard rotation while a tiny invisible
  anchor prop loads; this is normal.

---

## Credits & License

- **Author:** crxhvrd
- Built on **ScriptHookV** by Alexander Blade.
- Photo-capture / IGCS interop via the **IgcsConnector** ReShade add-on.

Released under the **MIT License** — see [`LICENSE`](LICENSE).
