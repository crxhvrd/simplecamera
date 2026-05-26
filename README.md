# Simple Camera — Cinematic Toolkit for GTA V

> [!NOTE]
> Supports **GTA V Legacy**, **GTA V Enhanced**, and **FiveM**.

A professional camera and world-control suite for Grand Theft Auto V. Built for filmmakers, photographers, and trailer editors who need precise, repeatable shots — not a casual flycam.

**Links** — [Source code on GitHub](https://github.com/crxhvrd/simplecamera) · [Join our Discord](https://discord.com/invite/jK4SRmBqYt)

**Headline features:**

- **Free Camera mode** — a 6-DOF flycam that can switch into a fully physics-based drone with tunable drag, acceleration, gravity, and auto-banking.
  - **Walk Mode** — terrain-following at a fixed eye height for ground-level tracking shots
  - **Acrobatic rotation engine** — quaternion-based, no gimbal lock when rolling through zenith
  - **Follow Target** — lock onto any ped, vehicle, or object with optional rigid-mount
  - **Procedural shake** — 5 named presets (Subtle, Handheld, Vehicle, Earthquake…) with speed-coupled amplitude and frequency
  - **Lens controls** — custom FOV, roll, zoom speed, look sensitivity, hide HUD / hide player
- **Camera Sequence mode** — author smooth keyframed camera animations with time-aware spline interpolation, ease curves, and effect events.
  - **Cubic Hermite splines** — velocity stays C1-continuous through every keyframe, even with mixed segment durations
  - **5 ease curves** — Linear, EaseInOut, EaseIn, EaseOut, Hold
  - **Seamless loop closure** — when first and last keyframes align, playback wraps with no snap and no velocity jerk
  - **Entity lock** — anchor keyframes to a moving vehicle or ped; the whole camera arc rides along with full position + rotation rigidity. Lag-free via native camera attachment during locked playback.
  - **Effect events** — scheduled shake changes, randomize triggers, and `World Speed` slow-mo / bullet-time
  - **F6 / F7 / F8 / F9 hotkeys** — capture, play, stop, jump to next keyframe
  - **In-world markers + path preview** — see your camera trajectory live in the world
- **Full time & weather control** — minute-accurate time of day, time-lapse, 14 weather types with blending, and world freeze.
  - **Pause / time-lapse** — freeze the clock or run Slow / Medium / Fast time-lapse modes
  - **Weather blending** — interpolate between any two of 14 weather types (e.g. "rain just starting")
  - **Freeze Entire World** — pause peds, vehicles, particles, and physics for clean composition
- **IGCS Connector support** — direct integration with ReShade's IGCS Connector addon for professional multi-frame depth-of-field photography.
  - **Multi-frame bokeh DoF** — high-quality out-of-focus rendering by sampling slightly offset camera positions
  - **Built-in DoF fallback** — simpler GTA-native depth-of-field with auto-focus, near/far range controls
  - **Automatic session handoff** — Sequence mode releases the IGCS lock cleanly so the workflow never conflicts

---

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Mode Picker — Two Camera Modes](#mode-picker--two-camera-modes)
- [Free Camera Mode](#free-camera-mode)
  - [Activation](#activation)
  - [Movement Settings](#movement-settings)
  - [Lens Settings](#lens-settings)
  - [Depth of Field](#depth-of-field)
  - [Camera Effects (Shake)](#camera-effects-shake)
  - [Time & Weather](#time--weather)
  - [Misc Settings](#misc-settings)
- [Camera Sequence Editor](#camera-sequence-editor)
  - [Authoring Workflow](#authoring-workflow)
  - [Sequence Menu Reference](#sequence-menu-reference)
  - [Pose Keyframes](#pose-keyframes)
  - [Effect Events](#effect-events)
  - [Entity Lock & Follow](#entity-lock--follow)
  - [Loop Closure](#loop-closure)
  - [Slow-Motion with World Speed](#slow-motion-with-world-speed)
  - [In-World Visualization](#in-world-visualization)
- [Specialized Modes](#specialized-modes)
  - [Drone Mode](#drone-mode)
  - [Walk Mode](#walk-mode)
  - [Follow Target](#follow-target)
  - [Rotation Engines](#rotation-engines)
- [IGCS / ReShade Depth of Field](#igcs--reshade-depth-of-field)
- [Controls Reference](#controls-reference)
- [Configuration](#configuration)
- [Common Workflows](#common-workflows)
- [Tips & Troubleshooting](#tips--troubleshooting)
- [Build & Development](#build--development)

---

## Installation

### Singleplayer (GTA V Legacy / Enhanced)

1. Install [ScriptHookV](http://www.dev-c.com/gtav/scripthookv/) and an ASI Loader.
2. Place `SimpleCamera.asi` in your main Grand Theft Auto V directory.
3. *(Optional)* For IGCS / professional DoF:
   - Install [ReShade](https://reshade.me/) with **full Add-on support**.
   - Place `IgcsConnector.addon64` in the game directory.
   - Install `IgcsDof.fx` under `reshade-shaders/shaders/`.
4. *(Optional)* Edit `SimpleCamera.ini` to remap the menu key or sequence hotkeys.

### FiveM

1. Copy `SimpleCamera.asi` into `FiveM\FiveM.app\plugins`.
2. For IGCS support, install the ReShade addon and shader in the same `plugins` folder.

> [!WARNING]
> Some FiveM servers reject scripts that mutate weather, time, or freeze entities. If you experience kicks, avoid the *Freeze Entire World*, *Time-of-Day Override*, and *Hide Player Character* features while connected.

---

## Quick Start

A five-minute tour to confirm everything works:

1. Press **F5** — the **Mode Picker** appears.
2. Choose **Free Camera**.
3. Press **F5** again to open the main menu — select **Toggle freecam**. The world freezes and the camera detaches.
4. Fly around with `W`/`A`/`S`/`D` + `Space`/`Ctrl`. Use the mouse to look around. Scroll the mouse wheel to adjust speed.
5. Press **F5**, scroll to **Lens settings**, and try the FOV slider for a quick zoom test.
6. Press **F5**, **Misc settings**, enable **Hide Game HUD** and **Hide Player Character** for a clean shot.
7. Snap a screenshot.

For cinematic camera moves, the next stop is the **Camera Sequence Editor** — covered in detail [below](#camera-sequence-editor).

---

## Mode Picker — Two Camera Modes

The first time you press **F5** after entering the world, you'll see the mode picker:

| Mode | Best for |
| :--- | :--- |
| **Free Camera** | Manual flight — photography, handheld-style shots, exploration. |
| **Camera Sequence** | Pre-planned, frame-accurate cinematic moves with keyframes. |

The two modes are mutually exclusive. To switch, open the menu and pick **EXIT** — the picker re-appears.

---

## Free Camera Mode

The main menu has 8 entries. Below each is a complete reference.

### Activation

> Menu: top-level → **Toggle freecam**

Toggles the free camera on and off. When ON:
- The player ped is frozen in place (unless **Allow Player to Move** is enabled in *Misc Settings*).
- The scripted camera takes over from the gameplay camera.
- Procedural shake noise seeds re-roll for a fresh pattern.

> [!TIP]
> When **Save Position on Exit** (in *Misc Settings*) is enabled, the camera re-spawns at the last position next time you toggle it on.

### Movement Settings

> Menu: top-level → **Movement**

This page controls how the camera flies. Some options have nested children that only appear when their parent is active (Walk Height shows under Walk Mode, drone tunables show under Drone Mode, etc.).

#### Core movement

| Setting | Range | What it does |
| :--- | :--- | :--- |
| **Camera Speed** | 0.1 – 50 | Base translation speed. The **mouse wheel** also adjusts this on the fly. |
| **Look Sensitivity** | 0.1 – 5 | Mouse / right-stick rotation gain. |
| **Zoom Speed** | 0.1 – 10 | Rate at which `+` / `-` / D-pad changes the FOV. |
| **Roll Speed** | 0.1 – 10 | Rate at which `Q` / `E` / LB/RB roll the camera. |
| **World Collision** | bool | When ON, the camera collides with geometry. When OFF (default), you fly through walls. |
| **Lock Altitude** | bool | Freezes the Z-axis — `Space` / `Ctrl` are ignored. Useful for level dolly shots. |

#### Walk Mode

| Setting | Range | What it does |
| :--- | :--- | :--- |
| **Walk Mode** | bool | Terrain-follows at a fixed eye height. |
| **Walk Height** | 0.1 – 50 m | Only visible when Walk Mode is ON. Distance above ground at the camera's current XY. Default 1.7 m (~5'7" eye level). |

#### Rotation engine

| Setting | Options | What it does |
| :--- | :--- | :--- |
| **Rotation Style** | Standard / Acrobatic | Switches between Euler and quaternion rotation. See [Rotation Engines](#rotation-engines). |

#### Follow target

| Setting | Options | What it does |
| :--- | :--- | :--- |
| **Follow Target** | None / Player / Aimed Entity | See [Follow Target](#follow-target). |
| **Rigid Mode** (sub) | bool | Camera inherits the target's rotation. |
| **Lock Entity** (sub) | action | Raycast forward and lock onto the entity at center-screen. |
| **Show Marker** (sub) | bool | Draws a marker above the locked entity. |

#### Movement style

| Setting | Options | What it does |
| :--- | :--- | :--- |
| **Movement Style** | Standard / Drone | Switches between direct positional control and physics-based flight. See [Drone Mode](#drone-mode). |

### Lens Settings

> Menu: top-level → **Lens settings**

| Setting | Range | What it does |
| :--- | :--- | :--- |
| **Lens Zoom (FOV)** | 1° – 130° | Field of view. Lower = telephoto / compression; higher = wide-angle. |
| **Lens Tilt (Roll)** | -180° – 180° | Camera roll angle. |

> [!TIP]
> A FOV in the 20–35° range gives a cinematic "long lens" feel. The default 50° is similar to a 35mm photo lens.

### Depth of Field

> Menu: top-level → **Depth of field**

The built-in DoF uses GTA V's native depth-of-field shader.

| Setting | Range | What it does |
| :--- | :--- | :--- |
| **Depth of Field** | bool | Master toggle. |
| **Auto-Focus** | bool | Focal distance tracks whatever the camera points at (via raycast). |
| **Manual Focus Dist.** | 0+ m | Distance to the focus plane. Ignored when Auto-Focus is ON. |
| **Near Focus Range** | 0+ m | Distance in front of the focus plane that stays sharp. |
| **Far Focus Range** | 0+ m | Distance behind the focus plane that stays sharp. |

For higher-quality / multi-frame bokeh DoF, use the [IGCS workflow](#igcs--reshade-depth-of-field) instead.

### Camera Effects (Shake)

> Menu: top-level → **Camera effects**

Procedural camera shake. A six-channel (3 position + 3 rotation) summed-sine noise field, 3 octaves per channel, designed to feel like a real handheld rig rather than a video-game wobble.

| Setting | Range | What it does |
| :--- | :--- | :--- |
| **Enabled** | bool | Master toggle. |
| **Preset** | Off / Subtle / Handheld / Vehicle / Earthquake | One-click sane defaults. See preset table below. |
| **Base Amplitude** | 0 – 5 | The baseline shake at rest. |
| **Base Frequency** | 0.5 – 30 Hz | The baseline oscillation rate. |
| **Speed → Amplitude** | 0 – 2 | How much shake intensifies as you move faster. 0 = independent of speed. |
| **Speed → Frequency** | 0 – 2 | How much shake speeds up as you move faster. |
| **Rotation Weight** | 0 – 2 | Multiplier on the rotational (pitch/yaw/roll) component. |
| **Position Weight** | 0 – 2 | Multiplier on the translational component. |
| **Stop When Still** | bool | Shake fades out when translation drops below ~0.05 m/s. Looking around in place does NOT count as motion. |
| **Randomize Pattern** | action | Re-rolls all noise seeds and per-axis frequency multipliers. Each session and each randomize fires a fresh-feeling pattern. |

#### Preset reference

| Preset | Amp | Freq | Speed→Amp | Speed→Freq | Use case |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Off** | 0.00 | 4.0 Hz | 0.0 | 0.0 | — |
| **Subtle** | 0.15 | 2.5 Hz | 0.5 | 0.3 | Tripod-with-breath, dialogue scenes |
| **Handheld** | 0.40 | 4.0 Hz | 1.0 | 0.5 | Documentary / follow-cam |
| **Vehicle** | 0.70 | 6.0 Hz | 1.5 | 1.0 | In-car POV, chases |
| **Earthquake** | 1.50 | 10 Hz | 0.0 | 0.0 | Disaster shots |

> [!TIP]
> The **Speed → Amp/Freq** couplings pair beautifully with Drone Mode — the faster you fly, the more the rig vibrates.

### Time & Weather

> Menu: top-level → **Time & weather**

| Setting | Range | What it does |
| :--- | :--- | :--- |
| **Pause Time of Day** | bool | Freezes the in-game clock at the current time. |
| **Time of Day** | 00:00 – 23:59 | Minute-accurate slider. Hold Left/Right to accelerate (1 min → 5 min → 15 min per step after 0.5s / 1.5s / 3s of holding). |
| **Time-lapse Speed** | Off / Slow / Medium / Fast | Multiplier on real-time clock advance. Time-lapse modes are independent of Pause. |
| **Primary Weather** | 14 types | Clear, Extra Sunny, Clouds, Overcast, Rain, Clearing, Thunder, Smog, Foggy, Christmas, Snow Light, Blizzard, Neutral, Snow. |
| **Secondary Weather** | 14 types | Same list — used for blending. |
| **Weather Blend Ratio** | 0% – 100% | Interpolation between primary and secondary. Lets you author "rain just starting" or "fog dissipating" looks. |
| **Apply Weather** | action | Pushes the current weather selection (or blend) to the engine. |
| **Freeze Entire World** | bool | Pauses peds, vehicles, particles, and physics. Pair with *Hide Player* for clean architectural shots. |

> [!TIP]
> Holding the Time-of-Day slider speeds up automatically — useful for big jumps (e.g., 12:00 → 19:30) without 450 individual nudges.

### Misc Settings

> Menu: top-level → **Misc settings**

| Setting | Default | What it does |
| :--- | :--- | :--- |
| **Hide Game HUD** | OFF | Hides the radar, weapon wheel, ammo, all HUD elements. Essential for clean screenshots and recordings. |
| **Hide Player Character** | OFF | Renders the player ped invisible. Combine with *Hide HUD* for unobstructed shots. |
| **Move Player with Camera** | OFF | The player ped follows the camera's XY position. Useful for "shadowing" shots where you want NPCs to react to the player being nearby. **Free Camera only.** |
| **Save Position on Exit** | OFF | When toggling free camera off and back on, the camera resumes at the last position instead of resetting. **Free Camera only.** |
| **Show Info Overlay** | OFF | Draws a small position / rotation / FOV / speed readout in the corner. Useful for debugging or matching shots. |
| **Lock Camera Position** | OFF | Freezes the camera in place — input is ignored. Lets you compose a shot, then lock it for time-lapse / IGCS work. **Free Camera only.** |
| **Allow Player to Move** | OFF | Un-freezes the player ped so they can walk/drive while you film. **Free Camera only.** |
| **Disable Vehicle Shake** | OFF | Suppresses the engine's built-in vehicle camera shake. |

---

## Camera Sequence Editor

The flagship feature. Authors a list of `PoseKeyframe`s along a timeline and plays them back through a smooth, time-weighted Hermite spline. A parallel **EffectEvent** track schedules shake / preset changes / world-speed transitions / pattern re-rolls to fire at specific moments.

Sequences persist to `SimpleCamera_Sequences.ini` next to the ASI.

### Authoring Workflow

The typical session:

1. Press **F5** → choose **Camera Sequence** from the mode picker.
2. The plugin enters Sequence mode and a default empty sequence is created if none exist.
3. **Fly the camera around in free-fly mode** — when playback is stopped, you can move the camera manually exactly like Free Camera mode.
4. Press **F6** (or *Capture Current Pose* in the menu) at each spot where you want a keyframe. The scrub cursor auto-advances **2 seconds** after each capture, so repeated F6 presses build evenly-spaced sequences.
5. Press **F7** to play. The camera smoothly interpolates through all keyframes using cubic Hermite splines.
6. Press **F8** to stop. Press **F7** again to resume.
7. Press **F9** to jump the scrub cursor to the next keyframe.
8. Open the menu, navigate to **Pose Keyframes** to edit times, ease curves, paths, or per-axis values.
9. Add **Effect Events** for shake / slow-mo / pattern changes scheduled at specific times.
10. **Save All to INI** when done. Sequences load automatically on next launch.

### Sequence Menu Reference

> Menu: top-level (in Sequence mode)

| Setting | What it does |
| :--- | :--- |
| **Sequence** | Cycles between saved sequences. Press Enter to create a new one. |
| **Play / Pause** | Toggles playback. |
| **Stop** | Stops playback and resets scrub time to 0. Restores the user's pre-play shake/world-speed state. |
| **Loop** | When ON, playback wraps at the end. Shows `On (closed)` if the loop is seamless — see [Loop Closure](#loop-closure). |
| **Speed** | Playback speed multiplier (0.05× – 8×). Independent of the world's time scale. |
| **Time** | Current scrub position / total duration. Press Enter to type a specific time. Left/Right nudge by 0.1s. |
| **Pose Keyframes…** | Opens the [Pose Keyframes](#pose-keyframes) list. |
| **Effect Events…** | Opens the [Effect Events](#effect-events) list. |
| **Capture Current Pose** | Equivalent to F6 — drops a new keyframe at the current scrub time. |
| **New Sequence** | Creates a new empty sequence. |
| **Delete Active Sequence** | Removes the currently selected sequence. |
| **Save All to INI** | Writes all sequences to `SimpleCamera_Sequences.ini`. |
| **Close Loop** | Snaps the last keyframe to match the first, or appends a closing keyframe. See [Loop Closure](#loop-closure). |
| **Follow & Entity Lock…** | Opens the [Entity Lock & Follow](#entity-lock--follow) submenu. Value column shows the current follow target + locked-keyframe count at a glance. |
| **Show Markers** | Toggles in-world keyframe visualization (the colored spheres + path lines). |
| **Hide HUD** | Hides the in-game HUD / radar. Mirrors the Free Camera *Misc Settings* toggle so you can clean the viewport without leaving Sequence mode. |
| **Hide Player Character** | Renders the player ped invisible. Toggle off when authoring sequences locked to the player ped so you can see it in frame. |
| **Exit** | Returns to the mode picker. |

> [!NOTE]
> The Sequence main menu has more rows than the on-screen viewport (12 visible). It **scrolls automatically** as you move the cursor — the title bar shows `[X-Y / total]` so you can tell what's off-screen.

### Pose Keyframes

> Menu: Sequence → **Pose Keyframes…**

Lists all keyframes in the active sequence with `time`, `FOV`, ease curve, and path type. The list **scrolls** — at most 12 rows are visible at once, with `▲ N more above` / `▼ N more below` indicators. The title bar shows the current viewport (`POSE KEYFRAMES [13-24 / 50]`).

Selecting a keyframe opens its per-pose editor with these fields:

| Field | What it does |
| :--- | :--- |
| **Time (s)** | When this keyframe fires on the timeline. Editable. |
| **Pos X / Y / Z** | World position. |
| **Pitch / Yaw / Roll** | World rotation in degrees. |
| **FOV** | Field of view at this keyframe. |
| **Ease** | Curve shape entering this keyframe. See ease types below. |
| **Path** | Linear (straight-line) or Spline (Hermite curve). See path types below. |
| **Entity Lock** | Press Enter to toggle. If unlocked and free-cam is currently following an entity (Player or Aimed), captures the lock onto that entity. If locked, releases. See [Entity Lock & Follow](#entity-lock--follow). Reads `Locked`, `Locked (entity gone)`, `Authored locked` (from a saved sequence — entity handles don't survive reloads), or `None`. |
| **Recapture from live** | Overwrites this keyframe's pos/rot/FOV with the current camera state *and* the current free-cam lock state. Useful for tweaking — fly to where you want, hit Enter. |
| **Delete** | Removes this keyframe. |

#### Ease types

| Ease | Behavior |
| :--- | :--- |
| **Linear** | Constant velocity through segment. |
| **EaseInOut** | Smoothstep — soft start, soft end. |
| **EaseIn** | Quadratic acceleration. |
| **EaseOut** | Quadratic deceleration. |
| **Hold** | Freeze on the previous keyframe; snap to this one on arrival. |

#### Path types

| Path | Behavior |
| :--- | :--- |
| **Linear** | Straight-line per-axis lerp; shortest-arc rotation. |
| **Spline** | Time-aware cubic Hermite across four control points. Velocity is C1-continuous across keyframes even when segment durations vary. |

Most plugins use Catmull-Rom uniform-parameter splines, which produce visible jerks when segment durations differ. This implementation uses **time-weighted finite-difference tangents**, so a 1-second segment followed by a 4-second segment still flows smoothly through the shared keyframe.

### Effect Events

> Menu: Sequence → **Effect Events…**

A parallel track of events that fire at specific timeline positions, independent of pose keyframes. The list **scrolls** with the same viewport pattern as Pose Keyframes.

#### Per-event fields

| Field | What it does |
| :--- | :--- |
| **Time (s)** | When this event fires. |
| **Effect** | The event kind — cycle through 11 options with Left/Right. |
| **Value** | Kind-dependent — see table below. |
| **Mode** | `Snap` (one-shot at event time) or `Ramp` (lerp from previous same-kind event across the gap). |
| **Delete** | Removes this event. |

#### Event kinds

| Kind | Value semantics |
| :--- | :--- |
| `Shake On/Off` | 0 or 1 |
| `Shake Preset` | 0 (Off) … 4 (Earthquake); auto-enables shake |
| `Shake Amp` | float — base amplitude |
| `Shake Freq` | Hz — base frequency |
| `Speed → Amp` | 0..2 — speed coupling |
| `Speed → Freq` | 0..2 — speed coupling |
| `Rotation Weight` | 0..2 |
| `Position Weight` | 0..2 |
| `Stop When Still` | 0 or 1 |
| `Randomize Pattern` | fire-once trigger; value ignored |
| **`World Speed`** | game time scale, clamped **0.1 – 1.0** — slow-mo / bullet-time |

#### Snap vs. Ramp

- **Snap** events apply once when the timeline crosses the event time. Use for instant changes — "shake on", "switch to Earthquake preset".
- **Ramp** events lerp from the most recent same-kind event's value to this event's value across the gap. Use for smooth transitions — "speed up shake over 2 seconds", "ease into slow-mo".

> [!NOTE]
> A snapshot of your authoring-time shake config is captured the moment **Play** starts. On **Stop** or end-of-playback, the original config is restored — so events scoped to the sequence don't leak into free-fly state.

### Entity Lock & Follow

> Menu: Sequence → **Follow & Entity Lock…**

Anchors keyframes to a moving entity (vehicle, ped, or object) so the entire camera arc rides along with it during playback. Designed for tracking shots — orbit a driving car, follow a walking ped, ride along with a flying jet — without rebuilding the sequence every time the subject moves.

**Mental model**

- Each keyframe optionally stores an **entity-local offset** (X, Y, Z) and an **entity rotation snapshot** at lock time.
- During playback, `ResolvePose` recomputes each control point's world position from `entity_current_position + rotated(localOffset)` every frame. Spline interpolation then runs on the live-resolved world coords.
- Rotation rides along via the **same additive-Euler-delta math** that Free Camera's rigid follow uses: `(pitch, yaw, roll) += (entity_rotation_now − lockEntRot)`.
- When both endpoints of a spline segment lock to the same entity, the driver uses GTA's native `ATTACH_CAM_TO_ENTITY` for position — eliminating the one-frame lag that `SET_CAM_COORD` exhibits on a fast-moving target. Lag-free tracking, same as free-cam rigid mode.

**The submenu**

| Row | Behavior |
| :--- | :--- |
| **Follow Target** | Cycle None / Player / Aimed Entity with Left/Right or Enter. Drives the same `g_FollowMode` as the free-cam menu. |
| **Lock Entity (raycast)** | *(Aimed only)* Fires a 1000 m ray from camera; locks onto whatever ped/vehicle/object is at center-screen. A white hover marker shows the candidate as you aim. Enter again to release. |
| **Lock to Nearest Vehicle** | *(Aimed only)* Foolproof fallback — `GET_CLOSEST_VEHICLE` within 30 m. Works even when the raycast misses (e.g. camera inside the target's collision volume on a close-up shot). |
| **Lock to Player's Vehicle** | *(Aimed only, when player is in a vehicle)* One-click "lock to the car I'm driving". |
| **Rigid Mode** | *(when locked)* Affects free-cam authoring — camera rotates with the entity while you compose. |
| **Show Marker** | *(when locked)* Draws a green marker above the locked entity so you can spot it in the world. |
| **Apply Lock to All Keyframes** | Re-anchors every keyframe in the active sequence to the current free-cam target. Each keyframe's local offset is computed from its stored world coords, so authored positions are preserved at the moment of binding. |
| **Clear All Keyframe Locks** | Drops the lock from every keyframe. World-space coords are preserved as fallback playback. |
| **Bake Locked Poses to World** | For every locked keyframe whose entity is live, writes the current world position into the keyframe's fallback. Pair with *Clear All Locks* for a permanent bake of the current arrangement. |

**Per-keyframe lock**

Open any keyframe in **Pose Keyframes** → the editor has an **Entity Lock** row. Enter toggles a single keyframe's lock without affecting the rest. Useful for sequences that mix locked tracking shots with static world-space shots.

**Authoring a tracking shot**

1. Free-cam → aim at the subject → lock the entity (or get in the vehicle, switch to Sequence mode, set Follow Target = Player).
2. Switch to **Camera Sequence** mode. The lock state carries over.
3. Fly the camera relative to the locked subject and press F6 at each spot — each keyframe captures both world coords *and* the entity-local offset / entity rotation snapshot.
4. Either capture all keyframes while locked, or capture them unlocked first and use **Apply Lock to All Keyframes** once you're happy with the world-space layout.
5. Play. The entire camera arc rides with the subject.

**Persistence notes**

- Saved sequences round-trip the offset + entity rotation snapshot through the INI. The **entity handle itself is NOT persisted** — GTA entity handles are session-scoped, so a stored handle would either be invalid or (worse) collide with an unrelated entity.
- On load, locked keyframes appear as `Authored locked` in the editor. To make them ride along again in this session, use **Apply Lock to All Keyframes** (or the per-keyframe Entity Lock toggle) to bind them to a current entity.
- If the locked entity is destroyed mid-playback, playback automatically falls through to the stored world-space coords — no crash, no jarring snap.

**The rigid-rotation trade-off**

When position is locked via native attach, GTA's procedural position-shake offsets are ignored (the engine owns position). Rotation shake still works. For a deliberately rigid tracking shot this is the right call — you usually don't want a "handheld jitter" component fighting a locked rig.

### Loop Closure

When the active sequence has `Loop = On` AND its first and last keyframes are within tolerance (default **1 m / 5° / 2° FOV**), the editor automatically enters **closed-loop mode**:

- The Loop value reads `On (closed)` in the menu.
- The first keyframe marker turns **cyan** and labels `KF 0 / Loop`.
- The seam segment's path-preview line tints cyan.
- Playback's last segment substitutes endpoint values from `pose[0]` — wrap from `t=duration → t=0` is bit-exact, no positional snap.
- Spline tangents at the seam wrap cyclically (`predecessor of pose[0] = pose[N-2]` shifted by `−duration`, `successor of pose[N-1] = pose[1]` shifted by `+duration`), so **velocity stays C1-continuous across the wrap**.

#### The "Close Loop" action

- If the last keyframe is already within tolerance, **hard-snaps** its pos/rot/FOV to exactly match `pose[0]` (the `Δ` readout drops to `0.00m / 0.0°`).
- Otherwise, **appends** a new closing keyframe at `last_t + avg_gap` with values copied from `pose[0]`. The new keyframe defaults to spline path + linear ease for a smooth wrap.

The menu's *Close Loop* row shows live gap diagnostics — e.g. `Δ0.42m / 3.1°` — so you can see how far off the seam is before acting.

### Slow-Motion with World Speed

The `World Speed` effect-event kind drives the game's `SET_TIME_SCALE` native. Use cases:

- **Snap to bullet-time** — Add an event at the desired moment, kind = `World Speed`, value = `0.25`, mode = `Snap`. The game instantly drops to 25% speed when the timeline crosses it.
- **Ease into slow-mo** — Two events:
  - Event A at `t = 0.0`, value = `1.0`, mode = `Ramp`.
  - Event B at `t = 2.0`, value = `0.3`, mode = `Ramp`.
  - The time scale smoothly lerps from 1.0× to 0.3× over those 2 seconds.
- **Ramp back to normal** — Pair a `0.3 → 1.0` ramp at the tail of the sequence for a graceful exit.

#### Behavior details

- Value clamped to **[0.1, 1.0]** at apply time. Values above 1.0 cause physics instability; very low values break animation.
- If a sequence fires any `World Speed` event, the time scale is automatically restored to **1.0×** when playback stops or ends.
- If no `World Speed` events are present, `SET_TIME_SCALE` is left untouched — preserving any external time-scale state owned by other scripts/mods.

### In-World Visualization

While in Sequence mode (toggle via *Show Markers*):

- Each keyframe renders as a colored sphere with a short arrow indicating its facing direction and a `KF N` label. Locked keyframes carry an `[L]` suffix on the label and **ride along with the entity** as it moves — the in-world markers reflect playback truth.
- **Color codes:**
  - **Magenta** — keyframe currently open in the editor.
  - **Yellow-green** — closest to the playback scrub position ("you are here").
  - **Cyan** — loop seam (when closed-loop mode is active).
  - **Teal** — keyframe locked to a live entity (ride-along).
  - **Olive** — normal world-space keyframe.
- The full camera path renders as a polyline; spline segments are tessellated so what you see matches what playback will do.
- **Effect events** project onto the path at their scheduled time with a compact label:
  - `Amp:0.40` — snap event.
  - `~Freq:6.0` — ramp event (the `~` indicates lerping).
  - Color-coded by family: warm orange = shake, icy blue = world speed, magenta = randomize.

> [!TIP]
> Turn markers off (`Show Markers = OFF`) when recording — `g_SequenceShowMarkers = false` removes all overlays so they don't appear in your clip.

---

## Specialized Modes

### Drone Mode

> Movement → **Movement Style** → `Drone`

A physics-based flight model. Switches WASD/stick inputs from "set position directly" to "apply thrust." When activated, six tunable parameters appear:

| Parameter | Range | Effect |
| :--- | :--- | :--- |
| **Drag** | 0 – 20 | Velocity damping. Lower = floatier; higher = more responsive stops. |
| **Acceleration** | 0 – 50 | Thrust gain per input axis. |
| **Gravity** | 0 – 20 | Downward pull when not thrusting up. 0 = neutral buoyancy. |
| **Banking** | 0 – 45° | Auto-roll angle when turning. Sells the FPV-drone feel. |
| **Rot. Smoothing** | 0 – 20 | Higher = snappier look response; lower = lazy/cinematic. |
| **FOV Smoothing** | 0 – 20 | Inertia on zoom transitions. |

Combine with the *Vehicle* shake preset for an authentic FPV racing look — speed-coupled shake amplifies as the drone accelerates.

### Walk Mode

> Movement → **Walk Mode**

Terrain-following at a fixed eye height. Useful for ground-level tracking shots without manually correcting altitude.

- **Walk Height** (default 1.7 m, range 0.1 – 50 m) — distance above ground at the camera's current XY.
- The **Walk Height** slider appears in the menu **only when Walk Mode is enabled** — keeps the menu compact when not in use.

### Follow Target

> Movement → **Follow Target**

The camera tracks a moving entity. Three modes:

- **None** — default, no tracking.
- **Player** — camera orbits the player ped. Optional **Rigid Mode** makes the camera inherit the ped's rotation (locked rig).
- **Aimed Entity** — raycast forward at center-screen; click to lock onto a ped/vehicle/object. Once locked:
  - **Lock Entity** — toggles between locked and unlocked.
  - **Show Marker** — draws a marker above the locked entity.
  - **Rigid Mode** — inherit the entity's rotation.

While unlocked, a hover marker appears on whatever's at the center of the screen — point at your subject before pressing Enter.

### Rotation Engines

> Movement → **Rotation Style**

- **Standard (Euler)** — pitch / yaw / roll applied independently using the engine's Rotation Order 2 (ZXY). Predictable for traditional camera moves; gimbal-locks at ±90° pitch (looking straight up or down).
- **Acrobatic (Quaternion)** — composes rotations as quaternions with explicit gimbal-lock handling. Lets you roll *through* the zenith without snapping. Ideal for barrel-roll / over-the-top / orbital moves. Slightly more compute-heavy but visually superior for complex motion.

> [!TIP]
> If you ever see the camera "flip" or "lock" when pitching past 90°, switch to Acrobatic.

---

## IGCS / ReShade Depth of Field

For multi-frame bokeh and cinematic out-of-focus rendering, the plugin integrates with the IGCS Connector ReShade addon.

### Setup

1. Install ReShade with full Add-on support.
2. Place `IgcsConnector.addon64` in your game directory (or FiveM `plugins` folder).
3. Install `IgcsDof.fx` in `reshade-shaders/shaders/`.

### Workflow

1. Activate **Free Camera** mode. (IGCS is gated to Free Camera — Sequence mode releases the lock.)
2. Open **ReShade** and enable the `IgcsDof` shader.
3. In the **Add-ons** tab, click **Start depth-of-field session**.
4. Set **Max Bokeh Size** (the image will split — that's expected).
5. Use the **Magnifier** to point at your subject.
6. Tune **Focus Delta X** until the magnifier image is sharp and unified.
7. Set quality (higher = more frames, sharper bokeh).
8. Click **Start Render**.

The plugin pushes the current camera state (position + rotation + FOV) to the shared IGCS buffer every frame so the ReShade addon can take multi-sample DoF frames from slightly offset positions.

> [!NOTE]
> When you switch to Camera Sequence mode, the plugin sends a "camera inactive" signal to IGCS so any active DoF session releases cleanly.

---

## Controls Reference

### Keyboard / Mouse

| Action | Key |
| :--- | :--- |
| Open / Close Menu | `F5` (rebindable) |
| Move | `W` `A` `S` `D` |
| Ascend / Descend | `Space` / `Ctrl` |
| Roll Left / Right | `Q` / `E` |
| Look Around | `Mouse` |
| Zoom In / Out | `+` / `-` |
| Adjust Speed | `Mouse Scroll` |
| **Sequence: Capture Pose** | `F6` |
| **Sequence: Play / Pause** | `F7` |
| **Sequence: Stop** | `F8` |
| **Sequence: Next Keyframe** | `F9` |

### Controller

| Action | Input |
| :--- | :--- |
| Move | `Left Stick` |
| Look Around | `Right Stick` |
| Move Up / Down | `RT` / `LT` |
| Roll Left / Right | `RB` / `LB` |
| Zoom In / Out | `D-Pad Up` / `Down` |
| Menu Navigation | `D-Pad` + `A` / `B` |

### Menu navigation

- `Up` / `Down` — move cursor through rows
- `Left` / `Right` — adjust slider values, cycle enum options, scrub time
- `Enter` (`A` on pad) — activate / open submenu / toggle
- `Back` (`B` on pad) — return to parent menu
- Long lists (Pose Keyframes / Effect Events) **auto-scroll** to keep the cursor in view

---

## Configuration

### `SimpleCamera.ini`

Located next to `SimpleCamera.asi`. Configures hotkeys via Windows virtual-key codes (full list [here](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)).

```ini
[Controls]
; Default F-keys (decimal VK codes)
MenuKey=116          ; F5  — main menu
SequenceAddKey=117   ; F6  — capture current pose
SequencePlayKey=118  ; F7  — play / pause
SequenceStopKey=119  ; F8  — stop
SequenceNextKey=120  ; F9  — jump to next keyframe
```

### `SimpleCamera_Sequences.ini`

Auto-generated when you *Save All to INI* in the Sequence menu. Format:

```ini
[Sequence:MySequenceName]
Loop=1
Speed=1.0000
PoseCount=4
Pose0=t=0.000,x=100.5,y=200.3,z=30.0,pitch=0,yaw=90,roll=0,fov=50,ease=0,path=1
Pose1=t=2.000,x=110.5,y=200.3,z=35.0,pitch=-10,yaw=90,roll=0,fov=45,ease=1,path=1
Pose2=t=4.000,x=120.5,y=200.3,z=35.0,pitch=-10,yaw=90,roll=0,fov=45,ease=1,path=1,locked=1,locX=2.500,locY=-3.000,locZ=1.200,lEntP=0.000,lEntY=180.000,lEntR=0.000
...
EventCount=2
Event0=t=1.000,kind=10,value=0.500,ramp=1
Event1=t=3.000,kind=10,value=1.000,ramp=1
```

One `[Sequence:Name]` section per sequence. Hand-editable if you need surgical control or want to programmatically generate sequences.

**Pose lock fields** (only present when the keyframe was authored with an entity lock):

- `locked=1` — flag indicating the keyframe was bound to an entity at authoring time.
- `locX,locY,locZ` — the camera's offset relative to the entity, in the entity's local frame.
- `lEntP,lEntY,lEntR` — the entity's world pitch / yaw / roll at lock time, used as the reference for the per-frame rotation delta during playback.

The **entity handle itself is intentionally NOT serialized** — GTA assigns handles per-session, so a saved handle would either be invalid or (worse) collide with an unrelated entity on load. After loading, the keyframe shows as *Authored locked* in the editor; bind it to a current entity via the Follow & Entity Lock submenu.

> [!CAUTION]
> Effect-event IDs were renumbered when shake replaced DoF/time/weather event kinds. Sequences saved with very old builds may have legacy events labeled `(legacy)` in the editor — delete them or remap their kind in the per-event submenu.

---

## Common Workflows

### Cinematic dolly past a building

1. Mode: **Free Camera**, fly to your starting position.
2. **Misc settings** → enable *Hide HUD*, *Hide Player Character*.
3. Switch to **Camera Sequence** mode.
4. F6 to capture start pose.
5. Fly to mid-shot position. F6.
6. Fly to end pose. F6.
7. Open **Pose Keyframes** → for each, set `Ease = EaseInOut`, `Path = Spline`.
8. F7 to preview. Adjust timing in the per-pose editor if pacing feels off.
9. **Save All to INI**.

### Bullet-time orbit

1. **Camera Sequence**, place 8 keyframes in a ring around a target (use Walk Mode to keep ground level).
2. Enable **Loop**, then **Close Loop** to make it seamless.
3. **Effect Events** → add `World Speed` event at `t=0`, value `1.0`, mode `Ramp`.
4. Add `World Speed` at `t=1.0`, value `0.3`, mode `Ramp` — fast ramp into bullet-time.
5. Add another `World Speed` at end of sequence, value `1.0`, mode `Ramp` — ease back out.
6. Play.

### Handheld documentary shot

1. **Camera Effects** → preset `Handheld`.
2. **Movement** → set Speed ~0.5 for a careful walking pace.
3. **Walk Mode ON**, Walk Height 1.6 m.
4. *Stop When Still* ON — so the shake settles when you stop moving.
5. Fly slowly through your scene.

### Tracking shot from a moving vehicle

1. **Camera Sequence** mode. Park the subject vehicle where you want the shot to start.
2. Open **Follow & Entity Lock…**
3. Either *Lock to Player's Vehicle* (one click if you're driving) or set Follow Target = *Aimed Entity* + *Lock to Nearest Vehicle*.
4. Fly the free-cam around the static car and press **F6** at each spot — the orbit, the over-the-shoulder, the side-tracking, etc. Each keyframe captures both world coords *and* the entity-relative offset.
5. *(Optional)* Hit **Apply Lock to All Keyframes** if you captured any before locking, to bind the full set in one shot.
6. *(Optional)* Toggle **Hide Player Character = OFF** if the ped should be visible in frame.
7. Press F7 to play, then start driving the car (or have a trainer autodrive it). The entire camera arc rides along with the vehicle — perfect framing through turns, accelerations, jumps.

### Time-lapse sunrise

1. **Time & weather** → set Time of Day to 05:30.
2. **Pause Time of Day** OFF.
3. **Time-lapse Speed** → `Medium`.
4. **Lock Camera Position** ON (compose your shot first, then lock).
5. **Hide HUD** ON.
6. Record.

---

## Tips & Troubleshooting

- **Camera jerks at keyframes** — Check that adjacent keyframes use `Path = Spline`. Linear paths produce velocity discontinuities at every keyframe.
- **Loop wrap visibly snaps** — Make sure `Loop = On (closed)` in the menu. If it reads `On (open)`, your first/last keyframes are too far apart — use *Close Loop* to fix.
- **Camera flips when looking straight up/down** — Switch *Rotation Style* to **Acrobatic**.
- **Shake feels wrong / too uniform** — Hit *Randomize Pattern* to re-roll the noise seeds. Each session gets a fresh pattern automatically, but you can re-roll mid-session.
- **In-game UI overlaps a menu I'm using** — Press `Esc` to close the GTA pause menu; the plugin's menu is rendered on top of normal HUD but not modal screens.
- **FiveM server kicked me** — Disable *Freeze Entire World*, *Time-of-Day*, and weather overrides while connected. Some servers are strict about engine-state mutations.
- **Sequence won't save** — Check that the game has write access to its install directory (Steam-installed games are usually fine; Microsoft Store / WSL installs may need permissions adjusted).
- **IGCS not connecting** — Ensure ReShade has *full Add-on support* enabled (the installer asks; default is "no"). The plugin polls for the addon — if it's loaded after the game starts, give it a few seconds.
- **Locked sequence shakes / lags on a moving car** — Make sure BOTH adjacent keyframes are locked to the SAME entity. The lag-free native-attach path only kicks in for fully-locked segments; mixed lock states fall back to script-driven `SET_CAM_COORD` which has a one-frame lag relative to entity physics.
- **Locked keyframes lost after reloading a sequence** — Expected: entity handles are session-scoped and never persisted to disk (a stored handle could collide with an unrelated entity). Re-bind via *Follow & Entity Lock → Apply Lock to All Keyframes* once you have an entity to anchor to.
- **Lock-to-Entity raycast keeps saying "No entity found"** — The camera is probably inside the target's collision volume. Either fly back a few meters, or use *Lock to Nearest Vehicle* / *Lock to Player's Vehicle* which don't depend on the raycast.

---

## Build & Development

### Project layout

```
FreeCameraPlugin/
├── camera.cpp / .h          # Free camera, drone physics, shake, walk mode
├── sequence.cpp / .h        # Keyframe editor, splines, loop closure, events
├── menu.cpp / .h            # In-game menu (NativeTrainer style)
├── igcs_bridge.cpp / .h     # IGCS Connector protocol
├── script.cpp / .h          # ScriptHookV main loop / mode dispatcher
├── keyboard.cpp / .h        # Keyboard input layer
├── main.cpp                 # DLL entry point
├── external/
│   ├── scripthook_sdk/      # ScriptHookV headers
│   └── IgcsConnector-main/  # IGCS protocol headers
└── bin/Release/SimpleCamera.asi  # Build output
```

### Build

Requires Visual Studio 2022 (v143 toolset) with the Desktop C++ workload. The project targets `Release|x64` only — there is no Debug configuration.

```powershell
msbuild FreeCameraPlugin.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Output: `bin\Release\SimpleCamera.asi`. Drop into your game folder.

### Architecture notes

- **Mode dispatcher** in `script.cpp` runs at every `WAIT(0)` and picks between `UpdateFreeCamera()` and `Sequence_FrameTick()` based on `g_CameraMode`.
- **Sequence playback** writes the camera transform directly via `SetCameraStateFromSequence()` and `SequencePushToEngine()` — it never re-enters the free-cam input/collision/follow code.
- **IGCS** is gated to Free Camera mode; in Sequence mode the plugin pushes a zero-state to release any active ReShade DoF session cleanly.
- **Spline math** lives in `HermiteSpline1D` (time-aware) and `unwrapAngle` (shortest-arc rotation across the ±180° seam).
- **Loop closure** is computed at runtime (`IsLoopClosedImpl`) from `loop=true` + first/last proximity within hardcoded tolerances (1 m / 5° / 2° FOV). Tolerances are at the top of `sequence.cpp` (`kLoopPosTolerance` etc.) if you want to tune them.
- **World Speed snapshot** uses a session-scoped flag (`s_WorldSpeedTouched`) — only restores `SET_TIME_SCALE` to 1.0× if the sequence actually fired a World Speed event, preserving external time-scale state otherwise.
- **List menu scrolling** is handled by `EnsureRowVisible(activeIdx, lineCount, scrollOffset)` — idempotent, called once per frame before rendering. Cap is `kMaxVisibleListRows = 12`. Used by Pose Keyframes, Effect Events, Movement Settings, and the Sequence main menu.
- **Entity lock** stores per-keyframe `entityHandle` + `localOffsetX/Y/Z` + `lockEntPitch/Yaw/Roll` (camera-relative-to-entity reference). `ResolvePose` is the single resolution point: when the keyframe's entity is live, it overwrites the returned copy's pos with `GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(...)` and adds the entity's rotation delta to pitch/yaw/roll. The spline interpolator downstream is oblivious to the lock — it just sees world-resolved coords.
- **Native-attach during locked playback** — when both endpoints of the active segment lock to the same entity, `ApplyPoseAtTime` converts the spline result back into entity-local space via `GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS` and calls `SetCameraStateFromSequenceLocked`. `SequencePushToEngine` then uses `ATTACH_CAM_TO_ENTITY` instead of `SET_CAM_COORD`, so the camera position is bound at render time (after physics) — eliminating the one-frame lag inherent in script-driven coord pushes on fast-moving entities.

---

*Developed with care for the GTA cinematography community.*
