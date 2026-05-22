/*
        GTA V Free Camera / Photo Mode Plugin
        Camera Sequence Mode

        Keyframe-driven camera animation. Authors a list of camera poses
        (PoseKeyframe) over a timeline and plays them back with configurable
        easing, optional spline interpolation, and a parallel track of
        EffectEvents (DoF, shake, time, weather) that fire on schedule.

        Lives alongside Free Camera mode; the top-level mode dispatcher
        decides which one drives the camera each frame.
*/

#pragma once

#include <string>
#include <vector>

// ============================================================
//  Public types
// ============================================================

enum EaseType {
  EASE_LINEAR = 0,
  EASE_IN_OUT = 1,
  EASE_IN = 2,
  EASE_OUT = 3,
  EASE_HOLD = 4, // freeze on previous keyframe until reaching this t
};

enum PathType {
  PATH_LINEAR = 0,
  PATH_SPLINE = 1, // Catmull-Rom across neighboring poses
};

enum EffectKind {
  EFX_SHAKE_ENABLED = 0, // value = 0 or 1
  EFX_SHAKE_PRESET = 1,        // value = 0..4 (preset index); auto-enables shake
  EFX_SHAKE_AMP = 2,           // float
  EFX_SHAKE_FREQ = 3,          // Hz
  EFX_SHAKE_SPEED_AMP = 4,     // speed→amplitude coupling (0..2)
  EFX_SHAKE_SPEED_FREQ = 5,    // speed→frequency coupling (0..2)
  EFX_SHAKE_ROT_WEIGHT = 6,    // rotation contribution (0..2)
  EFX_SHAKE_POS_WEIGHT = 7,    // position contribution (0..2)
  EFX_SHAKE_STOP_STILL = 8,    // stop-when-still toggle (0/1)
  EFX_SHAKE_RANDOMIZE = 9,     // fire = re-roll noise pattern (value ignored)
  EFX_WORLD_SPEED = 10,        // MISC::SET_TIME_SCALE(value); clamped 0.1..1.0
  // Note: enum values are stable IDs. Sequences saved with earlier
  // builds that used 4..11 for DOF / time / weather / walk / world-freeze
  // kinds now collide with the new shake kinds — open those events in
  // the editor and either delete them or set them to the right kind.
  // World Speed was added at the end (10) so existing saves keep working.
  EFX_COUNT = 11,
};

const char *EaseName(EaseType e);
const char *PathName(PathType p);
const char *EffectName(EffectKind k);

struct PoseKeyframe {
  float t;
  float posX, posY, posZ;
  float pitch, yaw, roll;
  float fov;
  EaseType ease;
  PathType path;
};

struct EffectEvent {
  float t;
  EffectKind kind;
  float value;
  bool ramp; // if true, lerp from previous event of same kind across [prev.t, this.t]
};

struct CameraSequence {
  std::string name;
  std::vector<PoseKeyframe> poses;
  std::vector<EffectEvent> events;
  bool loop;
  float playbackSpeed; // 1.0 = real-time
};

// ============================================================
//  Mode lifecycle
// ============================================================

// Called when the user enters Camera Sequence mode via the picker.
// Sets up the scripted camera; if no sequence exists yet, creates an empty one.
void Sequence_EnterMode();

// Called when leaving Camera Sequence mode.
// Stops playback, tears down the scripted camera.
void Sequence_ExitMode();

// Are we currently in Camera Sequence mode? (Set/cleared by Enter/ExitMode.)
bool Sequence_IsInMode();

// ============================================================
//  Per-frame tick
// ============================================================

// Called every frame from the main loop while in Sequence mode. Handles
// hotkeys, advances playback, applies pose + fires events, drives the
// scripted camera.
void Sequence_Tick(float dt);

// One-call per-frame driver for Sequence mode. Computes dt, runs
// Sequence_Tick, and when not playing also runs UpdateFreeCamera so the
// user can free-fly while authoring. Safe to call from either the main
// script loop OR a menu's inner draw loop — these contexts are mutually
// exclusive (ScriptHookV's WAIT(0) inside a menu does NOT re-enter main).
void Sequence_FrameTick();

// Is playback currently active (running, not paused)?
bool Sequence_IsPlaying();

// Total duration of the active sequence (max of last pose t and last event t).
float Sequence_TotalDuration();

// Current scrub / playback position in seconds.
float Sequence_CurrentTime();
void Sequence_SetCurrentTime(float t);

// ============================================================
//  Authoring / playback control
// ============================================================

void Sequence_Play();
void Sequence_Stop();
void Sequence_TogglePlay();
void Sequence_JumpToNextPose();
void Sequence_JumpToPrevPose();

// Capture current camera world pose into a new PoseKeyframe at the
// current scrub time (or at sequence end if no scrub). Returns the
// index of the inserted keyframe in the active sequence's pose list.
int Sequence_CapturePoseAtCurrentTime();

// Active sequence accessors (the one being edited/played)
CameraSequence *Sequence_Active();
int Sequence_ActiveIndex();
int Sequence_Count();
CameraSequence *Sequence_At(int index);
void Sequence_SetActive(int index);
void Sequence_New(const char *name);
void Sequence_DeleteActive();

// Pose / event mutations on the active sequence
void Sequence_DeletePose(int poseIdx);
void Sequence_AddEvent(EffectKind kind, float t, float value, bool ramp);
void Sequence_DeleteEvent(int eventIdx);
void Sequence_SortByTime(); // re-sort both arrays after time edits

// ============================================================
//  Persistence (SimpleCamera_Sequences.ini)
// ============================================================

void Sequence_LoadAll();
void Sequence_SaveAll();

// ============================================================
//  Loop closure
// ============================================================
//
// A sequence is "loop-closed" when its `loop` flag is true AND its
// first and last keyframes are close enough (position, rotation, FOV)
// that the wrap from t=duration back to t=0 should be seamless. When
// closed, the spline tangents at the seam wrap cyclically — predecessor
// of pose[0] reads pose[N-2] with a time offset of −duration, successor
// of pose[N-1] reads pose[1] with +duration — and the last segment's
// endpoint values are substituted from pose[0] so position lines up
// exactly. Velocity is C1-continuous across the wrap.
//
// Tolerances are hardcoded: 1 m position, 5° rotation, 2° FOV.

bool Sequence_IsLoopClosed();

struct LoopGap {
  float posDist;    // meters
  float pitchDelta; // degrees, abs, shortest-arc
  float yawDelta;
  float rollDelta;
  float fovDelta;
};
// Diagnostic gap between first and last keyframe. Returns false if the
// active sequence has fewer than 2 poses.
bool Sequence_GetLoopGap(LoopGap *out);

// Authoring action. If the active sequence has only one pose, clones
// it at t+2s. If the last pose is already within tolerance of the
// first, hard-snaps the last pose's values to exactly equal pose[0]
// (so the duplicate is bit-exact). Otherwise appends a new closing
// pose at end+avg_gap with values copied from pose[0]. Sets loop=true.
// Returns the resulting closing-pose index, or -1 if no active sequence.
int Sequence_CloseLoop();

// ============================================================
//  Hotkeys (read by Sequence_Tick)
// ============================================================

extern int g_SeqHotkeyAdd;     // default VK_F6
extern int g_SeqHotkeyPlay;    // default VK_F7
extern int g_SeqHotkeyStop;    // default VK_F8
extern int g_SeqHotkeyNext;    // default VK_F9

// Toggle for the in-world keyframe markers + path preview. Turn off
// when capturing a video / clip so the spheres and lines don't show up
// in the recording.
extern bool g_SequenceShowMarkers;

// Track which pose keyframe (if any) is currently open in the per-pose
// editor menu, so its in-world marker can be highlighted distinctly.
// -1 = nothing being edited.
void Sequence_SetEditingPose(int idx);
int Sequence_GetEditingPose();
