/*
        GTA V Free Camera / Photo Mode Plugin
        Camera Sequence Mode — implementation
*/

#include "sequence.h"

#include "camera.h"
#include "keyboard.h"

#include "external\scripthook_sdk\inc\main.h"
#include "external\scripthook_sdk\inc\natives.h"
#include "external\scripthook_sdk\inc\types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

#pragma warning(disable : 4244 4305)

// ============================================================
//  Constants / helpers
// ============================================================

static const float PI_F = 3.14159265358979323846f;
static const float DEG2RAD_F = PI_F / 180.0f;
static const float RAD2DEG_F = 180.0f / PI_F;

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Shortest-arc angle lerp in degrees. Handles wrap-around so that
// going from 170° → -170° travels 20° (through 180), not 340°.
static float lerpAngle(float a, float b, float t) {
  float d = b - a;
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return a + d * t;
}

const char *EaseName(EaseType e) {
  switch (e) {
  case EASE_LINEAR: return "Linear";
  case EASE_IN_OUT: return "EaseInOut";
  case EASE_IN:     return "EaseIn";
  case EASE_OUT:    return "EaseOut";
  case EASE_HOLD:   return "Hold";
  }
  return "?";
}

const char *PathName(PathType p) {
  return p == PATH_SPLINE ? "Spline" : "Linear";
}

const char *EffectName(EffectKind k) {
  switch (k) {
  case EFX_SHAKE_ENABLED:     return "Shake On/Off";
  case EFX_SHAKE_PRESET:      return "Shake Preset";
  case EFX_SHAKE_AMP:         return "Shake Amp";
  case EFX_SHAKE_FREQ:        return "Shake Freq";
  case EFX_SHAKE_SPEED_AMP:   return "Speed -> Amp";
  case EFX_SHAKE_SPEED_FREQ:  return "Speed -> Freq";
  case EFX_SHAKE_ROT_WEIGHT:  return "Rotation Weight";
  case EFX_SHAKE_POS_WEIGHT:  return "Position Weight";
  case EFX_SHAKE_STOP_STILL:  return "Stop When Still";
  case EFX_SHAKE_RANDOMIZE:   return "Randomize Pattern";
  case EFX_WORLD_SPEED:       return "World Speed";
  default:                    return "(unsupported)";
  }
}

// ============================================================
//  Module state
// ============================================================

int g_SeqHotkeyAdd = VK_F6;
int g_SeqHotkeyPlay = VK_F7;
int g_SeqHotkeyStop = VK_F8;
int g_SeqHotkeyNext = VK_F9;
bool g_SequenceShowMarkers = true;
static int s_EditingPoseIdx = -1;

// Snapshot of the user's shake configuration captured the moment Play
// starts. Effect events mutate the shake globals during playback; on
// Stop / end-of-playback we restore from this snapshot so the user's
// authoring-time shake settings come back unchanged. Without this,
// the camera would keep shaking forever after the sequence ends.
//
// Noise seeds and pattern multipliers are intentionally NOT included —
// the user typically wants any Randomize firing to leave a fresh
// pattern in place.
struct ShakeSnapshot {
  bool enabled;
  int preset;
  float amp, freq;
  float spdAmp, spdFreq;
  float rotW, posW;
  bool stopStill;
};
static ShakeSnapshot s_PreplayShake;
static bool s_HasShakeSnapshot = false;

// Tracks whether the current playback session has fired at least one
// World Speed event. If so, RestoreShakeSnapshot pushes the time scale
// back to 1.0 on Stop / end-of-playback so the user's authoring state
// isn't left in slow-motion. If no World Speed event fired, we don't
// touch SET_TIME_SCALE at all — preserving any time scale the user (or
// another script) may have set outside the sequence.
static bool s_WorldSpeedTouched = false;

static void SaveShakeSnapshot() {
  s_PreplayShake.enabled = g_ShakeEnabled;
  s_PreplayShake.preset = g_ShakePreset;
  s_PreplayShake.amp = g_ShakeAmp;
  s_PreplayShake.freq = g_ShakeFreq;
  s_PreplayShake.spdAmp = g_ShakeSpeedAmpCoupling;
  s_PreplayShake.spdFreq = g_ShakeSpeedFreqCoupling;
  s_PreplayShake.rotW = g_ShakeRotWeight;
  s_PreplayShake.posW = g_ShakePosWeight;
  s_PreplayShake.stopStill = g_ShakeStopWhenStill;
  s_HasShakeSnapshot = true;
  s_WorldSpeedTouched = false;
}

static void RestoreShakeSnapshot() {
  if (!s_HasShakeSnapshot) return;
  g_ShakeEnabled = s_PreplayShake.enabled;
  g_ShakePreset = s_PreplayShake.preset;
  g_ShakeAmp = s_PreplayShake.amp;
  g_ShakeFreq = s_PreplayShake.freq;
  g_ShakeSpeedAmpCoupling = s_PreplayShake.spdAmp;
  g_ShakeSpeedFreqCoupling = s_PreplayShake.spdFreq;
  g_ShakeRotWeight = s_PreplayShake.rotW;
  g_ShakePosWeight = s_PreplayShake.posW;
  g_ShakeStopWhenStill = s_PreplayShake.stopStill;
  s_HasShakeSnapshot = false;
  // If the session pushed any World Speed events, snap the time scale
  // back to real-time. Otherwise leave SET_TIME_SCALE alone so we don't
  // stomp on time-scale state owned by other scripts/mods.
  if (s_WorldSpeedTouched) {
    GAMEPLAY::SET_TIME_SCALE(1.0f);
    s_WorldSpeedTouched = false;
  }
}

void Sequence_SetEditingPose(int idx) { s_EditingPoseIdx = idx; }
int Sequence_GetEditingPose() { return s_EditingPoseIdx; }

static std::vector<CameraSequence> s_Sequences;
static int s_ActiveIdx = -1;
static bool s_InMode = false;
static bool s_Playing = false;
static bool s_FirstPlayTick = false; // widen event window on the very first tick
static float s_PlaybackTime = 0.0f;
static float s_LastTickTime = 0.0f; // for event cross-detection
static DWORD s_LastFrameTick = 0;

// We forward the playback override to the camera via this flag so
// UpdateFreeCamera (if it runs in some edge case) won't fight us.
// Also used so the sequence mode menu / authoring code knows.
extern bool g_SequencePlaybackActive; // defined in camera.cpp

// Forward decl — defined later, used by Sequence_SetCurrentTime for
// scrub-preview pose application.
static void ApplyPoseAtTime(float t, const CameraSequence *s);

// Forward decl — defined alongside event firing, used by Sequence_Play
// for catch-up when starting playback mid-sequence.
static void ApplyEffectValue(EffectKind kind, float value);

// ============================================================
//  Easing
// ============================================================

static float ApplyEase(EaseType e, float t) {
  t = clampf(t, 0.0f, 1.0f);
  switch (e) {
  case EASE_LINEAR:
    return t;
  case EASE_IN_OUT:
    // Smoothstep (cubic Hermite): 3t^2 - 2t^3
    return t * t * (3.0f - 2.0f * t);
  case EASE_IN:
    // Quadratic ease-in
    return t * t;
  case EASE_OUT:
    // Quadratic ease-out
    return 1.0f - (1.0f - t) * (1.0f - t);
  case EASE_HOLD:
    // Stay at 0 until t reaches 1, then snap
    return t >= 1.0f ? 1.0f : 0.0f;
  }
  return t;
}

// ============================================================
//  Quaternion slerp for rotation interpolation
//  (Uses the same Euler convention as camera.cpp's Quat helpers)
// ============================================================

struct SQuat {
  float w, x, y, z;
};

static SQuat EulerToSQuat(float pitchDeg, float yawDeg, float rollDeg) {
  // Match the convention used in camera.cpp:EulerToQuat — ZXY order
  // (yaw about Z, pitch about X, roll about Y), with yaw negated.
  float halfYaw = -yawDeg * DEG2RAD_F * 0.5f;
  float halfPitch = pitchDeg * DEG2RAD_F * 0.5f;
  float halfRoll = rollDeg * DEG2RAD_F * 0.5f;
  SQuat qYaw   = {cosf(halfYaw),   0.0f,             0.0f,           sinf(halfYaw)};
  SQuat qPitch = {cosf(halfPitch), sinf(halfPitch),  0.0f,           0.0f};
  SQuat qRoll  = {cosf(halfRoll),  0.0f,             sinf(halfRoll), 0.0f};
  // qYaw * qPitch
  SQuat qa;
  qa.w = qYaw.w*qPitch.w - qYaw.x*qPitch.x - qYaw.y*qPitch.y - qYaw.z*qPitch.z;
  qa.x = qYaw.w*qPitch.x + qYaw.x*qPitch.w + qYaw.y*qPitch.z - qYaw.z*qPitch.y;
  qa.y = qYaw.w*qPitch.y - qYaw.x*qPitch.z + qYaw.y*qPitch.w + qYaw.z*qPitch.x;
  qa.z = qYaw.w*qPitch.z + qYaw.x*qPitch.y - qYaw.y*qPitch.x + qYaw.z*qPitch.w;
  // qa * qRoll
  SQuat r;
  r.w = qa.w*qRoll.w - qa.x*qRoll.x - qa.y*qRoll.y - qa.z*qRoll.z;
  r.x = qa.w*qRoll.x + qa.x*qRoll.w + qa.y*qRoll.z - qa.z*qRoll.y;
  r.y = qa.w*qRoll.y - qa.x*qRoll.z + qa.y*qRoll.w + qa.z*qRoll.x;
  r.z = qa.w*qRoll.z + qa.x*qRoll.y - qa.y*qRoll.x + qa.z*qRoll.w;
  return r;
}

static void SQuatToEuler(const SQuat &q, float &pitchOut, float &yawOut,
                         float &rollOut) {
  // Inverse of EulerToSQuat (ZXY). Lifted from camera.cpp:QuatToEuler.
  float m12 = 2.0f * (q.x * q.y - q.z * q.w);
  float m22 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
  float m31 = 2.0f * (q.x * q.z - q.y * q.w);
  float m32 = 2.0f * (q.y * q.z + q.x * q.w);
  float m33 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

  pitchOut = asinf(clampf(m32, -1.0f, 1.0f)) * RAD2DEG_F;
  if (fabsf(m32) < 0.999f) {
    yawOut = atan2f(-m12, m22) * RAD2DEG_F;
    rollOut = atan2f(-m31, m33) * RAD2DEG_F;
  } else {
    yawOut = 0.0f;
    rollOut = 0.0f;
  }
}

static SQuat Slerp(SQuat a, SQuat b, float t) {
  float dot = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
  if (dot < 0.0f) {
    b.w = -b.w; b.x = -b.x; b.y = -b.y; b.z = -b.z;
    dot = -dot;
  }
  if (dot > 0.9995f) {
    // Nearly identical → linear lerp + renormalize
    SQuat r = {a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t,
               a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    float len = sqrtf(r.w*r.w + r.x*r.x + r.y*r.y + r.z*r.z);
    if (len > 0.00001f) { r.w/=len; r.x/=len; r.y/=len; r.z/=len; }
    return r;
  }
  float theta = acosf(dot);
  float sinTheta = sinf(theta);
  float wa = sinf((1.0f - t) * theta) / sinTheta;
  float wb = sinf(t * theta) / sinTheta;
  return {a.w*wa + b.w*wb, a.x*wa + b.x*wb,
          a.y*wa + b.y*wb, a.z*wa + b.z*wb};
}

// ============================================================
//  Catmull-Rom position spline (kept for reference; superseded by
//  HermiteSpline1D below which handles non-uniform time spacing).
// ============================================================

static float CatmullRom1D(float p0, float p1, float p2, float p3, float t) {
  float t2 = t * t;
  float t3 = t2 * t;
  return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f*p0 - 5.0f*p1 + 4.0f*p2 - p3) * t2 +
                 (-p0 + 3.0f*p1 - 3.0f*p2 + p3) * t3);
}

// ============================================================
//  Time-aware cubic Hermite spline
// ============================================================
//
// Given 4 control points (p0..p3) at times (t0..t3) and a query time t
// in [t1, t2], compute a smooth interpolated value. The tangents at p1
// and p2 are finite-difference slopes weighted by the actual time
// spacing — so the tangent at p1 reaching one segment FROM behind
// equals the tangent leaving INTO the next segment. Result: velocity
// is C1-continuous across keyframes even when segment durations vary.
//
// Catmull-Rom (uniform parameter) only achieves position continuity
// when all segments have equal duration; with mixed durations the
// camera visibly jerks at keyframes. Hermite with time-weighted slopes
// is the standard fix.
static float HermiteSpline1D(float p0, float p1, float p2, float p3,
                             float t0, float t1, float t2, float t3,
                             float t) {
  float du = t2 - t1;
  if (du < 0.0001f) return p1;
  float dt02 = t2 - t0;
  float dt13 = t3 - t1;
  float m1 = (dt02 > 0.0001f) ? (p2 - p0) / dt02 : 0.0f;
  float m2 = (dt13 > 0.0001f) ? (p3 - p1) / dt13 : 0.0f;
  float u = (t - t1) / du;
  if (u < 0.0f) u = 0.0f;
  if (u > 1.0f) u = 1.0f;
  float u2 = u * u;
  float u3 = u2 * u;
  float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
  float h10 = u3 - 2.0f * u2 + u;
  float h01 = -2.0f * u3 + 3.0f * u2;
  float h11 = u3 - u2;
  return h00 * p1 + h10 * du * m1 + h01 * p2 + h11 * du * m2;
}

// Unwrap `angle` so it's within ±180° of `reference`. Used so Hermite
// rotation interpolation takes the short way around when angles cross
// the ±180 seam.
static float unwrapAngle(float angle, float reference) {
  while (angle - reference > 180.0f) angle -= 360.0f;
  while (angle - reference < -180.0f) angle += 360.0f;
  return angle;
}

// ============================================================
//  Loop closure — tolerances + detector + cyclic neighbor lookup
// ============================================================

// Tuned by feel: a 1m gap is small enough that the eye doesn't catch
// it at the wrap, 5° is below the threshold most users notice for
// rotation continuity, 2° FOV is roughly one click in the in-game menu.
static const float kLoopPosTolerance = 1.0f;
static const float kLoopRotTolerance = 5.0f;
static const float kLoopFovTolerance = 2.0f;

static bool IsLoopClosedImpl(const CameraSequence *s) {
  if (!s || !s->loop || s->poses.size() < 2) return false;
  const PoseKeyframe &a = s->poses.front();
  const PoseKeyframe &b = s->poses.back();
  float dx = b.posX - a.posX, dy = b.posY - a.posY, dz = b.posZ - a.posZ;
  if (sqrtf(dx*dx + dy*dy + dz*dz) > kLoopPosTolerance) return false;
  // unwrapAngle returns b's angle re-expressed within ±180 of a, so the
  // delta is the shortest-arc difference.
  if (fabsf(unwrapAngle(b.pitch, a.pitch) - a.pitch) > kLoopRotTolerance) return false;
  if (fabsf(unwrapAngle(b.yaw,   a.yaw)   - a.yaw)   > kLoopRotTolerance) return false;
  if (fabsf(unwrapAngle(b.roll,  a.roll)  - a.roll)  > kLoopRotTolerance) return false;
  if (fabsf(b.fov - a.fov) > kLoopFovTolerance) return false;
  return true;
}

bool Sequence_IsLoopClosed() { return IsLoopClosedImpl(Sequence_Active()); }

bool Sequence_GetLoopGap(LoopGap *out) {
  if (!out) return false;
  CameraSequence *s = Sequence_Active();
  if (!s || s->poses.size() < 2) return false;
  const PoseKeyframe &a = s->poses.front();
  const PoseKeyframe &b = s->poses.back();
  float dx = b.posX - a.posX, dy = b.posY - a.posY, dz = b.posZ - a.posZ;
  out->posDist = sqrtf(dx*dx + dy*dy + dz*dz);
  out->pitchDelta = fabsf(unwrapAngle(b.pitch, a.pitch) - a.pitch);
  out->yawDelta   = fabsf(unwrapAngle(b.yaw,   a.yaw)   - a.yaw);
  out->rollDelta  = fabsf(unwrapAngle(b.roll,  a.roll)  - a.roll);
  out->fovDelta   = fabsf(b.fov - a.fov);
  return true;
}

// Resolves a control-point index for the spline tangent calc. For
// loop-closed sequences, indices wrap cyclically over poses[0..N-2]
// (the canonical loop — poses[N-1] is a duplicate of poses[0]) and
// the returned tOffset shifts the control point's time value by
// ±duration so time-aware Hermite slopes stay correct across the seam.
// For non-closed sequences this just clamps to [0, N-1] with zero offset.
struct CtrlRef {
  int idx;
  float tOffset;
};

static CtrlRef ResolveCtrl(int idx, int N, bool closed, float dur) {
  CtrlRef r{idx, 0.0f};
  if (!closed) {
    if (r.idx < 0) r.idx = 0;
    if (r.idx >= N) r.idx = N - 1;
    return r;
  }
  int eff = N - 1; // unique poses in the cycle
  if (eff < 1) { r.idx = 0; return r; }
  while (r.idx < 0)    { r.idx += eff; r.tOffset -= dur; }
  while (r.idx >= eff) { r.idx -= eff; r.tOffset += dur; }
  return r;
}

// Resolves a control-point's pos/rot/fov values. When closed and the
// resolved index lands on the seam duplicate (N-1), substitutes pose[0]'s
// values — that's what makes the wrap bit-exact even if the user hasn't
// hard-snapped the last keyframe yet. Returns a copy by value so the
// caller can read .posX / .pitch / .fov / etc. directly without further
// indirection.
static PoseKeyframe ResolvePose(const CameraSequence *s, int idx,
                                bool closed) {
  int N = (int)s->poses.size();
  PoseKeyframe p = s->poses[idx];
  if (closed && idx == N - 1) {
    const PoseKeyframe &q = s->poses[0];
    p.posX = q.posX; p.posY = q.posY; p.posZ = q.posZ;
    p.pitch = q.pitch; p.yaw = q.yaw; p.roll = q.roll;
    p.fov = q.fov;
  }
  return p;
}

// ============================================================
//  Sequence accessors
// ============================================================

static CameraSequence *EnsureActive() {
  if (s_Sequences.empty()) {
    CameraSequence s;
    s.name = "Default";
    s.loop = false;
    s.playbackSpeed = 1.0f;
    s_Sequences.push_back(s);
    s_ActiveIdx = 0;
  } else if (s_ActiveIdx < 0 || s_ActiveIdx >= (int)s_Sequences.size()) {
    s_ActiveIdx = 0;
  }
  return &s_Sequences[s_ActiveIdx];
}

CameraSequence *Sequence_Active() {
  if (s_Sequences.empty() || s_ActiveIdx < 0 ||
      s_ActiveIdx >= (int)s_Sequences.size())
    return nullptr;
  return &s_Sequences[s_ActiveIdx];
}

int Sequence_ActiveIndex() { return s_ActiveIdx; }
int Sequence_Count() { return (int)s_Sequences.size(); }

CameraSequence *Sequence_At(int index) {
  if (index < 0 || index >= (int)s_Sequences.size())
    return nullptr;
  return &s_Sequences[index];
}

void Sequence_SetActive(int index) {
  if (index >= 0 && index < (int)s_Sequences.size()) {
    s_ActiveIdx = index;
    Sequence_Stop();
  }
}

void Sequence_New(const char *name) {
  CameraSequence s;
  s.name = (name && *name) ? name : "Untitled";
  s.loop = false;
  s.playbackSpeed = 1.0f;
  s_Sequences.push_back(s);
  s_ActiveIdx = (int)s_Sequences.size() - 1;
  Sequence_Stop();
}

void Sequence_DeleteActive() {
  if (s_ActiveIdx < 0 || s_ActiveIdx >= (int)s_Sequences.size())
    return;
  s_Sequences.erase(s_Sequences.begin() + s_ActiveIdx);
  if (s_Sequences.empty()) {
    s_ActiveIdx = -1;
    EnsureActive();
  } else if (s_ActiveIdx >= (int)s_Sequences.size()) {
    s_ActiveIdx = (int)s_Sequences.size() - 1;
  }
  Sequence_Stop();
}

void Sequence_SortByTime() {
  CameraSequence *s = Sequence_Active();
  if (!s) return;
  std::sort(s->poses.begin(), s->poses.end(),
            [](const PoseKeyframe &a, const PoseKeyframe &b) { return a.t < b.t; });
  std::sort(s->events.begin(), s->events.end(),
            [](const EffectEvent &a, const EffectEvent &b) { return a.t < b.t; });
}

int Sequence_CloseLoop() {
  CameraSequence *s = Sequence_Active();
  if (!s) return -1;
  // Closing a non-looping sequence makes no sense — flip the flag so
  // playback actually wraps. (The user can still toggle Loop back off
  // afterward if they only wanted the value-snap.)
  s->loop = true;
  if (s->poses.empty()) return -1;

  if (s->poses.size() == 1) {
    // Trivial: duplicate the single pose 2s later. The two-pose result
    // is automatically loop-closed by definition.
    PoseKeyframe k = s->poses[0];
    k.t = s->poses[0].t + 2.0f;
    s->poses.push_back(k);
    Sequence_SortByTime();
    return (int)s->poses.size() - 1;
  }

  // If already within tolerance, hard-snap the last pose's values to
  // exactly match the first. This makes the duplicate bit-exact (and is
  // visible to the user — the deltas drop to 0 in the menu).
  if (IsLoopClosedImpl(s)) {
    PoseKeyframe &last = s->poses.back();
    const PoseKeyframe &first = s->poses.front();
    last.posX = first.posX; last.posY = first.posY; last.posZ = first.posZ;
    last.pitch = first.pitch; last.yaw = first.yaw; last.roll = first.roll;
    last.fov = first.fov;
    return (int)s->poses.size() - 1;
  }

  // Otherwise append a new closing pose at last.t + average_gap with
  // values copied from pose[0]. The average gap heuristic keeps the new
  // seam segment's duration consistent with the rest of the sequence,
  // so playback speed across the wrap feels natural.
  float lastT = s->poses.back().t;
  float avgGap = 2.0f;
  if (s->poses.size() >= 2) {
    avgGap = (s->poses.back().t - s->poses.front().t) /
             (float)(s->poses.size() - 1);
    if (avgGap < 0.5f) avgGap = 0.5f;
  }
  PoseKeyframe k = s->poses.front();
  k.t = lastT + avgGap;
  // Spline path for the seam keeps the closing arc smooth; Linear ease
  // gives the constant-velocity wrap we want for a continuous loop.
  k.path = PATH_SPLINE;
  k.ease = EASE_LINEAR;
  s->poses.push_back(k);
  Sequence_SortByTime();
  return (int)s->poses.size() - 1;
}

void Sequence_DeletePose(int idx) {
  CameraSequence *s = Sequence_Active();
  if (!s || idx < 0 || idx >= (int)s->poses.size())
    return;
  s->poses.erase(s->poses.begin() + idx);
}

void Sequence_AddEvent(EffectKind kind, float t, float value, bool ramp) {
  CameraSequence *s = EnsureActive();
  EffectEvent e = {t, kind, value, ramp};
  s->events.push_back(e);
  Sequence_SortByTime();
}

void Sequence_DeleteEvent(int idx) {
  CameraSequence *s = Sequence_Active();
  if (!s || idx < 0 || idx >= (int)s->events.size())
    return;
  s->events.erase(s->events.begin() + idx);
}

// ============================================================
//  Time / duration / scrub
// ============================================================

float Sequence_TotalDuration() {
  CameraSequence *s = Sequence_Active();
  if (!s) return 0.0f;
  float maxT = 0.0f;
  for (const auto &p : s->poses) if (p.t > maxT) maxT = p.t;
  for (const auto &e : s->events) if (e.t > maxT) maxT = e.t;
  return maxT;
}

float Sequence_CurrentTime() { return s_PlaybackTime; }

void Sequence_SetCurrentTime(float t) {
  if (t < 0.0f) t = 0.0f;
  float dur = Sequence_TotalDuration();
  if (t > dur) t = dur;
  s_LastTickTime = t;
  s_PlaybackTime = t;
  // Apply the pose at this time immediately so the user sees what's
  // there while scrubbing. Pass dt=0 to suppress shake — a one-shot
  // scrub apply shouldn't animate jitter.
  CameraSequence *s = Sequence_Active();
  if (s && !s->poses.empty()) {
    ApplyPoseAtTime(t, s);
    SequencePushToEngine(0.0f);
  }
}

bool Sequence_IsPlaying() { return s_Playing; }

void Sequence_Play() {
  CameraSequence *s = Sequence_Active();
  if (!s || s->poses.empty()) return;
  if (s_PlaybackTime >= Sequence_TotalDuration())
    s_PlaybackTime = 0.0f;

  // Snapshot the user's shake config the moment Play starts (only on
  // first transition into play — Pause+Play preserves the original
  // snapshot so events stay scoped to the sequence).
  if (!s_HasShakeSnapshot) SaveShakeSnapshot();

  // Catch-up: if Play starts past t=0 (user scrubbed forward first), apply
  // every snap event whose time is <= the current scrub position so the
  // engine state matches "what would have happened if we played from 0".
  // Without this, the DoF / shake / weather state at the moment of Play
  // ignores all the events you scrolled past.
  for (const auto &e : s->events) {
    if (e.ramp) continue;
    if (e.t <= s_PlaybackTime) {
      ApplyEffectValue(e.kind, e.value);
    }
  }
  // Apply the visual pose at the current scrub position too. dt=0
  // suppresses shake on this initial push; the first real Sequence_Tick
  // call will start animating shake with the actual frame delta.
  ApplyPoseAtTime(s_PlaybackTime, s);
  SequencePushToEngine(0.0f);

  s_LastTickTime = s_PlaybackTime;
  s_FirstPlayTick = true;
  s_Playing = true;
  g_SequencePlaybackActive = true;
}

void Sequence_Stop() {
  s_Playing = false;
  s_PlaybackTime = 0.0f;
  s_LastTickTime = 0.0f;
  g_SequencePlaybackActive = false;
  // Roll back any shake mutations applied by effect events during this
  // playback session so authoring/free-fly state isn't polluted.
  RestoreShakeSnapshot();
}

void Sequence_TogglePlay() {
  if (s_Playing) {
    s_Playing = false;
    g_SequencePlaybackActive = false;
  } else {
    Sequence_Play();
  }
}

void Sequence_JumpToNextPose() {
  CameraSequence *s = Sequence_Active();
  if (!s) return;
  for (const auto &p : s->poses) {
    if (p.t > s_PlaybackTime + 0.001f) {
      Sequence_SetCurrentTime(p.t);
      return;
    }
  }
}

void Sequence_JumpToPrevPose() {
  CameraSequence *s = Sequence_Active();
  if (!s) return;
  float best = -1.0f;
  for (const auto &p : s->poses) {
    if (p.t < s_PlaybackTime - 0.001f && p.t > best)
      best = p.t;
  }
  if (best >= 0.0f) Sequence_SetCurrentTime(best);
  else Sequence_SetCurrentTime(0.0f);
}

int Sequence_CapturePoseAtCurrentTime() {
  CameraSequence *s = EnsureActive();
  float px, py, pz, pitch, yaw, roll;
  GetCameraState(px, py, pz, pitch, yaw, roll);

  PoseKeyframe k;
  k.t = s_PlaybackTime;
  // If there's already a pose with this t, nudge so we don't create
  // coincident keyframes (zero-length segment + identical display).
  // 0.05s is enough that %.2f rendering shows distinct values.
  for (const auto &existing : s->poses) {
    if (fabsf(existing.t - k.t) < 0.05f) {
      k.t = existing.t + 0.05f;
    }
  }
  k.posX = px; k.posY = py; k.posZ = pz;
  k.pitch = pitch; k.yaw = yaw; k.roll = roll;
  k.fov = g_CamFOV;
  // Spline + Linear is the smoothest default: time-aware Hermite path
  // through the surrounding keyframes with constant-velocity progress
  // through each segment. Users who want a different feel can change
  // ease/path in the per-pose editor.
  k.ease = EASE_LINEAR;
  k.path = PATH_SPLINE;
  s->poses.push_back(k);
  Sequence_SortByTime();

  // Auto-advance scrub by 2s past the new pose. The next capture (whether
  // from the menu button or the F6 hotkey) naturally lands 2s later, so
  // "fly → F6 → fly → F6" builds an evenly-spaced sequence. Done here
  // (not at the call sites) so both paths behave identically.
  s_PlaybackTime = k.t + 2.0f;
  s_LastTickTime = s_PlaybackTime;

  // Find the new pose's index after sorting
  for (int i = 0; i < (int)s->poses.size(); ++i)
    if (fabsf(s->poses[i].t - k.t) < 0.0001f) return i;
  return -1;
}

// ============================================================
//  Playback: pose interpolation
// ============================================================

static void ApplyPoseAtTime(float t, const CameraSequence *s) {
  if (!s || s->poses.empty()) return;

  // Single pose: just hold it
  if (s->poses.size() == 1) {
    const PoseKeyframe &p = s->poses[0];
    SetCameraStateFromSequence(p.posX, p.posY, p.posZ, p.pitch, p.yaw, p.roll,
                                p.fov);
    return;
  }

  // Find the segment [i, i+1] containing t
  int i = 0;
  for (int k = 0; k < (int)s->poses.size() - 1; ++k) {
    if (t >= s->poses[k].t && t <= s->poses[k + 1].t) {
      i = k; break;
    }
    if (k == (int)s->poses.size() - 2 && t > s->poses[k + 1].t) {
      i = k; // past end, clamp at last segment
    }
  }
  if (t < s->poses.front().t) i = 0;

  const int N = (int)s->poses.size();
  const bool closed = IsLoopClosedImpl(s);
  // Cycle period for closed loops: the seam pose's time. (We could
  // alternatively use TotalDuration, but pose[N-1].t is the canonical
  // wrap point since events past it don't make sense in a closed loop.)
  const float dur = closed ? s->poses[N - 1].t : 0.0f;

  // Resolve a and b. When closed and we're on the seam segment
  // [N-2, N-1], pose[N-1]'s values are substituted from pose[0] so the
  // wrap is bit-exact even if the user hasn't manually snapped them.
  const PoseKeyframe a = ResolvePose(s, i, closed);
  const PoseKeyframe b = ResolvePose(s, i + 1, closed);
  float segDur = b.t - a.t;
  float u = (segDur > 0.0001f) ? clampf((t - a.t) / segDur, 0.0f, 1.0f) : 0.0f;
  float te = ApplyEase(b.ease, u);

  float px, py, pz;
  float pitch, yaw, roll;

  if (b.path == PATH_SPLINE && N >= 2) {
    // Time-aware cubic Hermite across 4 control points (i-1, i, i+1, i+2).
    // For non-closed sequences, boundary clamping produces one-sided
    // slopes at start/end. For closed sequences, ResolveCtrl wraps the
    // neighbor indices and returns a time offset (±dur) so the slope
    // calculation sees the cyclic continuation — making velocity C1-
    // continuous across the seam.
    CtrlRef cp0 = ResolveCtrl(i - 1, N, closed, dur);
    CtrlRef cp3 = ResolveCtrl(i + 2, N, closed, dur);
    const PoseKeyframe p0 = ResolvePose(s, cp0.idx, closed);
    const PoseKeyframe p3 = ResolvePose(s, cp3.idx, closed);
    const float p0t = p0.t + cp0.tOffset;
    const float p3t = p3.t + cp3.tOffset;

    // We pass eased time into the spline via a scaled `t` value so the
    // ease shape (Linear / EaseInOut / etc.) reshapes progress through
    // the segment without breaking position continuity at boundaries.
    float tEased = a.t + te * (b.t - a.t);

    px = HermiteSpline1D(p0.posX, a.posX, b.posX, p3.posX,
                         p0t, a.t, b.t, p3t, tEased);
    py = HermiteSpline1D(p0.posY, a.posY, b.posY, p3.posY,
                         p0t, a.t, b.t, p3t, tEased);
    pz = HermiteSpline1D(p0.posZ, a.posZ, b.posZ, p3.posZ,
                         p0t, a.t, b.t, p3t, tEased);

    // Apply the same spline treatment to rotation. Unwrap angles
    // relative to the segment endpoints (a, b) so the spline takes the
    // short way around the ±180° seam instead of swinging through.
    float a_pitch = a.pitch;
    float b_pitch = unwrapAngle(b.pitch, a_pitch);
    float p0_pitch = unwrapAngle(p0.pitch, a_pitch);
    float p3_pitch = unwrapAngle(p3.pitch, b_pitch);

    float a_yaw = a.yaw;
    float b_yaw = unwrapAngle(b.yaw, a_yaw);
    float p0_yaw = unwrapAngle(p0.yaw, a_yaw);
    float p3_yaw = unwrapAngle(p3.yaw, b_yaw);

    float a_roll = a.roll;
    float b_roll = unwrapAngle(b.roll, a_roll);
    float p0_roll = unwrapAngle(p0.roll, a_roll);
    float p3_roll = unwrapAngle(p3.roll, b_roll);

    pitch = HermiteSpline1D(p0_pitch, a_pitch, b_pitch, p3_pitch,
                            p0t, a.t, b.t, p3t, tEased);
    yaw = HermiteSpline1D(p0_yaw, a_yaw, b_yaw, p3_yaw,
                          p0t, a.t, b.t, p3t, tEased);
    roll = HermiteSpline1D(p0_roll, a_roll, b_roll, p3_roll,
                           p0t, a.t, b.t, p3t, tEased);
  } else {
    // Linear path: simple lerp per axis, shortest-arc for rotation.
    px = lerpf(a.posX, b.posX, te);
    py = lerpf(a.posY, b.posY, te);
    pz = lerpf(a.posZ, b.posZ, te);
    pitch = lerpAngle(a.pitch, b.pitch, te);
    yaw   = lerpAngle(a.yaw,   b.yaw,   te);
    roll  = lerpAngle(a.roll,  b.roll,  te);
  }

  float fov = lerpf(a.fov, b.fov, te);

  SetCameraStateFromSequence(px, py, pz, pitch, yaw, roll, fov);
}

// ============================================================
//  Playback: event firing
// ============================================================

static void ApplyEffectValue(EffectKind kind, float value) {
  switch (kind) {
  case EFX_SHAKE_ENABLED:
    g_ShakeEnabled = value >= 0.5f;
    break;
  case EFX_SHAKE_PRESET: {
    int p = (int)(value + 0.5f);
    if (p >= 0 && p <= 4) {
      ApplyShakePreset(p);
      // Auto-enable shake when picking a non-Off preset (otherwise the
      // event silently writes amp/freq but nothing visible happens).
      // Off preset (0) leaves enabled state alone — explicit disable is
      // via SHAKE_ENABLED=0 event.
      if (p > 0) g_ShakeEnabled = true;
    }
    break;
  }
  case EFX_SHAKE_AMP:
    g_ShakeAmp = value;
    break;
  case EFX_SHAKE_FREQ:
    g_ShakeFreq = value;
    break;
  case EFX_SHAKE_SPEED_AMP:
    g_ShakeSpeedAmpCoupling = value;
    break;
  case EFX_SHAKE_SPEED_FREQ:
    g_ShakeSpeedFreqCoupling = value;
    break;
  case EFX_SHAKE_ROT_WEIGHT:
    g_ShakeRotWeight = value;
    break;
  case EFX_SHAKE_POS_WEIGHT:
    g_ShakePosWeight = value;
    break;
  case EFX_SHAKE_STOP_STILL:
    g_ShakeStopWhenStill = value >= 0.5f;
    break;
  case EFX_SHAKE_RANDOMIZE:
    // Fire-once trigger: re-roll the noise seeds. Value is meaningless.
    // If the user marked this event as `ramp` it'll re-roll every frame
    // during the ramp window — that produces a chaotic white-noise look
    // (sometimes desirable). For a single re-roll, use snap mode.
    RandomizeShakePattern();
    break;
  case EFX_WORLD_SPEED: {
    // Slow-motion / hyperlapse via GAMEPLAY::SET_TIME_SCALE. Clamp to
    // [0.1, 1.0] — values above 1.0 cause physics instability and the
    // game's animation system handles values below ~0.05 poorly.
    // Ramp mode produces smooth speed transitions (e.g. ease into bullet
    // time over 0.5s); snap mode flips instantly.
    float v = value;
    if (v < 0.1f) v = 0.1f;
    if (v > 1.0f) v = 1.0f;
    GAMEPLAY::SET_TIME_SCALE(v);
    s_WorldSpeedTouched = true;
    break;
  }
  default:
    // Removed kinds (DoF, time, weather, walk mode, world freeze) are
    // silently ignored if they appear in an old saved sequence.
    break;
  }
}

// For ramp events: find the previous event of the same kind (by index in
// the active sequence's event list). If found, returns its value at
// `out_prevVal` and its time at `out_prevT`; returns true on success.
static bool FindPrevEventOfKind(const CameraSequence *s, int curIdx,
                                EffectKind kind, float &out_prevT,
                                float &out_prevVal) {
  for (int i = curIdx - 1; i >= 0; --i) {
    if (s->events[i].kind == kind) {
      out_prevT = s->events[i].t;
      out_prevVal = s->events[i].value;
      return true;
    }
  }
  return false;
}

// Fire snap events whose t falls in (lastT, nowT]; for ramp events that
// are currently "in flight" (i.e. lastSameKind.t < nowT < this.t), apply
// the lerped value continuously.
// `firstTick` widens the lower bound to include lastT itself, so events
// scheduled exactly at the playback start time (e.g. t=0) fire on the
// very first frame instead of being skipped.
static void DriveEvents(const CameraSequence *s, float lastT, float nowT,
                        bool firstTick) {
  if (!s) return;

  // Pass 1: snap events. Window is (lastT, nowT] in normal flow, but
  // [lastT, nowT] on the first tick so t=0 events fire.
  for (int i = 0; i < (int)s->events.size(); ++i) {
    const EffectEvent &e = s->events[i];
    if (e.ramp) continue;
    bool inWindow = firstTick ? (e.t >= lastT && e.t <= nowT)
                              : (e.t > lastT && e.t <= nowT);
    if (inWindow) {
      ApplyEffectValue(e.kind, e.value);
    }
  }

  // Pass 2: ramp events — for each ramp event, if nowT is within its
  // active span (prevSameKind.t .. e.t], lerp.
  for (int i = 0; i < (int)s->events.size(); ++i) {
    const EffectEvent &e = s->events[i];
    if (!e.ramp) continue;
    float prevT, prevVal;
    if (!FindPrevEventOfKind(s, i, e.kind, prevT, prevVal)) {
      // No predecessor — treat as snap at e.t
      if (e.t > lastT && e.t <= nowT) ApplyEffectValue(e.kind, e.value);
      continue;
    }
    if (nowT >= prevT && nowT <= e.t && e.t > prevT) {
      float u = (nowT - prevT) / (e.t - prevT);
      ApplyEffectValue(e.kind, lerpf(prevVal, e.value, u));
    } else if (nowT > e.t && lastT <= e.t) {
      // Just crossed the end of the ramp — snap to final value
      ApplyEffectValue(e.kind, e.value);
    }
  }
}

// ============================================================
//  Mode lifecycle + per-frame tick
// ============================================================

bool Sequence_IsInMode() { return s_InMode; }

void Sequence_EnterMode() {
  if (s_InMode) return;
  s_InMode = true;
  s_LastFrameTick = 0;
  EnsureActive();
  // Use Free Camera's scripted-camera infrastructure so the view actually
  // gets taken over. We never run UpdateFreeCamera in this mode — the
  // sequence tick drives s_Pos* directly via SetCameraStateFromSequence
  // and pushes to the engine.
  if (!g_FreeCamActive) {
    InitFreeCamera();
  }
  // Disable HUD by default (consistent with Free Camera) and detach any
  // follow / drone state that would fight playback.
  g_FollowMode = 0;
  g_FollowTargetEntity = 0;
  g_DroneMode = false;
}

void Sequence_ExitMode() {
  if (!s_InMode) return;
  Sequence_Stop();
  s_InMode = false;
  if (g_FreeCamActive) {
    DestroyFreeCamera();
  }
}

// ============================================================
//  In-world keyframe visualization
// ============================================================
//
// Draws a sphere at each pose keyframe and a thin polyline between
// consecutive poses so the user can see the camera path. The pose
// closest to the current scrub time is highlighted (larger + brighter)
// to act as a "you are here" cursor.
//
// For PATH_SPLINE segments we tessellate the Catmull-Rom curve so the
// preview line matches what playback will actually do.

// World→screen text helper. Returns silently if the point is offscreen
// or behind the camera (_WORLD3D_TO_SCREEN2D returns FALSE in both cases).
static void DrawText3D(float x, float y, float z, const char *text, int r,
                       int g, int b, int a, float scale = 0.30f) {
  float sx, sy;
  if (!GRAPHICS::_WORLD3D_TO_SCREEN2D(x, y, z, &sx, &sy)) return;
  UI::SET_TEXT_FONT(0);
  UI::SET_TEXT_SCALE(0.0f, scale);
  UI::SET_TEXT_COLOUR(r, g, b, a);
  UI::SET_TEXT_CENTRE(1);
  UI::SET_TEXT_DROPSHADOW(2, 0, 0, 0, 200);
  UI::SET_TEXT_EDGE(1, 0, 0, 0, 220);
  UI::_SET_TEXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)text);
  UI::_DRAW_TEXT(sx, sy);
}

// Cheap position-only interpolation along a sequence's path. Used for
// placing event labels along the camera's trajectory at the event's
// scheduled time. Linear lerp is fine for label positioning; we don't
// need spline accuracy here.
static void ComputePosAtTime(float t, const CameraSequence *s,
                             float &outX, float &outY, float &outZ) {
  outX = outY = outZ = 0.0f;
  if (!s || s->poses.empty()) return;
  if (s->poses.size() == 1) {
    outX = s->poses[0].posX;
    outY = s->poses[0].posY;
    outZ = s->poses[0].posZ;
    return;
  }
  int i = 0;
  for (int k = 0; k < (int)s->poses.size() - 1; ++k) {
    if (t >= s->poses[k].t && t <= s->poses[k + 1].t) { i = k; break; }
    if (k == (int)s->poses.size() - 2 && t > s->poses[k + 1].t) i = k;
  }
  if (t < s->poses.front().t) i = 0;
  const bool closed = IsLoopClosedImpl(s);
  const PoseKeyframe a = ResolvePose(s, i, closed);
  const PoseKeyframe b = ResolvePose(s, i + 1, closed);
  float segDur = b.t - a.t;
  float u = (segDur > 0.0001f) ? clampf((t - a.t) / segDur, 0.0f, 1.0f) : 0.0f;
  outX = lerpf(a.posX, b.posX, u);
  outY = lerpf(a.posY, b.posY, u);
  outZ = lerpf(a.posZ, b.posZ, u);
}

// Compact, human-readable one-line summary of an event for the in-world
// label. Prefixed with "~" for ramp events so the visual distinguishes
// snap vs. lerp at a glance.
static std::string FormatEventLabel(const EffectEvent &e) {
  char buf[64];
  const char *p = e.ramp ? "~" : "";
  switch (e.kind) {
  case EFX_SHAKE_ENABLED:
    sprintf_s(buf, "%sShake %s", p, e.value >= 0.5f ? "On" : "Off"); break;
  case EFX_SHAKE_PRESET: {
    static const char *names[] = {"Off", "Subtle", "Handheld", "Vehicle",
                                  "Earthquake"};
    int idx = (int)(e.value + 0.5f);
    if (idx < 0) idx = 0; if (idx > 4) idx = 4;
    sprintf_s(buf, "%sPreset:%s", p, names[idx]); break;
  }
  case EFX_SHAKE_AMP:        sprintf_s(buf, "%sAmp:%.2f",     p, e.value); break;
  case EFX_SHAKE_FREQ:       sprintf_s(buf, "%sFreq:%.1f",    p, e.value); break;
  case EFX_SHAKE_SPEED_AMP:  sprintf_s(buf, "%sSpdAmp:%.2f",  p, e.value); break;
  case EFX_SHAKE_SPEED_FREQ: sprintf_s(buf, "%sSpdFreq:%.2f", p, e.value); break;
  case EFX_SHAKE_ROT_WEIGHT: sprintf_s(buf, "%sRotW:%.2f",    p, e.value); break;
  case EFX_SHAKE_POS_WEIGHT: sprintf_s(buf, "%sPosW:%.2f",    p, e.value); break;
  case EFX_SHAKE_STOP_STILL:
    sprintf_s(buf, "%sStopStill %s", p, e.value >= 0.5f ? "On" : "Off"); break;
  case EFX_SHAKE_RANDOMIZE:
    sprintf_s(buf, "%sRandomize!", p); break;
  case EFX_WORLD_SPEED:
    sprintf_s(buf, "%sSpeed:%.2fx", p, e.value); break;
  default: sprintf_s(buf, "%s(legacy)", p); break;
  }
  return buf;
}

static void DrawSequenceMarkers() {
  if (!g_SequenceShowMarkers) return;
  CameraSequence *s = Sequence_Active();
  if (!s || s->poses.empty()) return;

  const int N = (int)s->poses.size();
  const bool closed = IsLoopClosedImpl(s);

  // Find the pose nearest in time to the scrub cursor
  int currentIdx = 0;
  float bestDt = 1e9f;
  for (int i = 0; i < N; ++i) {
    float d = fabsf(s->poses[i].t - s_PlaybackTime);
    if (d < bestDt) { bestDt = d; currentIdx = i; }
  }
  // Only treat as "current" if reasonably close (within 0.5s); otherwise
  // we're between keyframes and nothing should be highlighted.
  bool hasCurrent = bestDt < 0.5f;

  // Markers — type 28 = sphere. States:
  //  - loop seam (closed && i == 0): cyan, larger — the merged endpoint
  //  - editing (s_EditingPoseIdx == i): bright magenta — the menu's open
  //  - current (closest to scrub): yellow-green — playback "you are here"
  //  - normal: dim olive
  // Editing wins over current when both apply. When the loop is closed,
  // we skip the duplicate last keyframe's marker entirely (it would draw
  // on top of pose[0]) — pose[0] gets the cyan "Loop ⟲" treatment instead.
  for (int i = 0; i < N; ++i) {
    if (closed && i == N - 1) continue; // duplicate of pose[0]; rendered there
    const PoseKeyframe &p = s->poses[i];
    bool isEditing = (i == s_EditingPoseIdx);
    bool isCurrent = hasCurrent &&
                     (i == currentIdx ||
                      // Treat seam dup as the same logical pose as 0 for
                      // highlighting purposes — otherwise "current" would
                      // visibly flicker off pose[0] when scrub lands on
                      // the seam.
                      (closed && i == 0 && currentIdx == N - 1));
    bool isSeam = closed && i == 0;
    int r, g, b;
    float size;
    if (isEditing)      { r = 255; g =   0; b = 255; size = 0.55f; }
    else if (isCurrent) { r = 255; g = 220; b =   0; size = 0.45f; }
    else if (isSeam)    { r =   0; g = 220; b = 255; size = 0.45f; }
    else                { r =  80; g = 200; b =  80; size = 0.30f; }
    int a = s_Playing ? 90 : 200;
    if (isEditing) a = 230; // always vivid so the editor target is unmistakable
    if (isSeam && !s_Playing) a = 230;
    GRAPHICS::DRAW_MARKER(28, p.posX, p.posY, p.posZ, 0.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, size, size, size, r, g, b, a,
                          FALSE, FALSE, 2, FALSE, NULL, NULL, FALSE);
    // Small forward-direction indicator: a tiny line from the marker out
    // in the keyframe's facing direction so the user can see orientation
    float yawRad = p.yaw * (3.14159265f / 180.0f);
    float pitchRad = p.pitch * (3.14159265f / 180.0f);
    float dx = -sinf(yawRad) * cosf(pitchRad);
    float dy = cosf(yawRad) * cosf(pitchRad);
    float dz = sinf(pitchRad);
    const float arrowLen = isEditing ? 1.2f : 0.8f;
    GRAPHICS::DRAW_LINE(p.posX, p.posY, p.posZ,
                        p.posX + dx * arrowLen, p.posY + dy * arrowLen,
                        p.posZ + dz * arrowLen, r, g, b, a);
    // Label above the marker. Seam pose gets the "Loop" tag so the user
    // can see at a glance which sequences are closed.
    char poseLabel[32];
    if (isSeam) sprintf_s(poseLabel, "KF 0 / Loop");
    else        sprintf_s(poseLabel, "KF %d", i);
    int la = s_Playing ? 140 : 230;
    DrawText3D(p.posX, p.posY, p.posZ + 0.5f, poseLabel, r, g, b, la, 0.32f);
  }

  // Path preview — line strip between consecutive poses. For spline
  // segments we sample the curve so what you see matches what playback
  // will do. When the loop is closed, the seam segment [N-2, N-1] uses
  // cyclic neighbors (matches ApplyPoseAtTime) so the preview line
  // curves smoothly back to the start.
  const int subdivPerSegment = 16;
  const float dur = closed ? s->poses[N - 1].t : 0.0f;
  for (int i = 0; i + 1 < N; ++i) {
    const PoseKeyframe a = ResolvePose(s, i, closed);
    const PoseKeyframe b = ResolvePose(s, i + 1, closed);
    int alpha = s_Playing ? 80 : 140;
    // Seam segment gets the cyan tint so it visually pops as the loop arc.
    bool seamSegment = closed && (i + 1) == N - 1;
    int lineR = seamSegment ? 0   : 100;
    int lineG = seamSegment ? 220 : 180;
    int lineB = seamSegment ? 255 : 255;

    if (b.path == PATH_SPLINE) {
      CtrlRef cp0 = ResolveCtrl(i - 1, N, closed, dur);
      CtrlRef cp3 = ResolveCtrl(i + 2, N, closed, dur);
      const PoseKeyframe p0 = ResolvePose(s, cp0.idx, closed);
      const PoseKeyframe p3 = ResolvePose(s, cp3.idx, closed);
      const float p0t = p0.t + cp0.tOffset;
      const float p3t = p3.t + cp3.tOffset;
      float prevX = a.posX, prevY = a.posY, prevZ = a.posZ;
      for (int k = 1; k <= subdivPerSegment; ++k) {
        float u = (float)k / (float)subdivPerSegment;
        float tEased = a.t + u * (b.t - a.t);
        float x = HermiteSpline1D(p0.posX, a.posX, b.posX, p3.posX,
                                  p0t, a.t, b.t, p3t, tEased);
        float y = HermiteSpline1D(p0.posY, a.posY, b.posY, p3.posY,
                                  p0t, a.t, b.t, p3t, tEased);
        float z = HermiteSpline1D(p0.posZ, a.posZ, b.posZ, p3.posZ,
                                  p0t, a.t, b.t, p3t, tEased);
        GRAPHICS::DRAW_LINE(prevX, prevY, prevZ, x, y, z,
                            lineR, lineG, lineB, alpha);
        prevX = x; prevY = y; prevZ = z;
      }
    } else {
      GRAPHICS::DRAW_LINE(a.posX, a.posY, a.posZ, b.posX, b.posY, b.posZ,
                          lineR, lineG, lineB, alpha);
    }
  }

  // Event text labels — projected onto the path at each event's scheduled
  // time. Stacks of labels at the same time get a small vertical fan so
  // they don't overlap. Color codes the event family.
  float lastT = -1.0f;
  int stackCount = 0;
  for (int i = 0; i < (int)s->events.size(); ++i) {
    const EffectEvent &e = s->events[i];
    float ex, ey, ez;
    ComputePosAtTime(e.t, s, ex, ey, ez);
    if (fabsf(e.t - lastT) < 0.05f) {
      stackCount++;
    } else {
      stackCount = 0;
      lastT = e.t;
    }
    // Warm orange for shake events; bright magenta for the one-shot
    // Randomize trigger so it visually pops; muted grey for any
    // legacy/removed kinds loaded from older sequence files so they're
    // visibly inert.
    int r, g, b;
    switch (e.kind) {
    case EFX_SHAKE_ENABLED:    case EFX_SHAKE_PRESET:
    case EFX_SHAKE_AMP:        case EFX_SHAKE_FREQ:
    case EFX_SHAKE_SPEED_AMP:  case EFX_SHAKE_SPEED_FREQ:
    case EFX_SHAKE_ROT_WEIGHT: case EFX_SHAKE_POS_WEIGHT:
    case EFX_SHAKE_STOP_STILL:
      r = 255; g = 160; b = 80; break;
    case EFX_SHAKE_RANDOMIZE:
      r = 255; g = 80;  b = 220; break;
    case EFX_WORLD_SPEED:
      // Cool icy blue — visually reads as "time / slow-mo" and stays
      // distinct from the warm-orange shake family and magenta randomize.
      r = 100; g = 200; b = 255; break;
    default:
      r = 140; g = 140; b = 140; break;
    }
    int a = s_Playing ? 130 : 220;
    std::string label = FormatEventLabel(e);
    // 0.9m above the path is well clear of the KF labels (0.5m offset).
    // Each stacked label adds another 0.3m.
    float zOff = 0.9f + 0.3f * stackCount;
    DrawText3D(ex, ey, ez + zOff, label.c_str(), r, g, b, a, 0.28f);
    // Small downward tick from label to the path point, so the user can
    // see which spot along the path the event fires at.
    GRAPHICS::DRAW_LINE(ex, ey, ez, ex, ey, ez + zOff - 0.1f,
                        r, g, b, a / 2);
  }
}

void Sequence_FrameTick() {
  if (!s_InMode) return;
  float dt = GAMEPLAY::GET_FRAME_TIME();
  if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
  Sequence_Tick(dt);
  // Free-fly authoring: when playback is OFF, let the user fly the camera
  // around with WASD/etc. to compose the next keyframe. UpdateFreeCamera
  // owns input + collision + IGCS + shake handling.
  if (!s_Playing && g_FreeCamActive) {
    UpdateFreeCamera();
  }
}

void Sequence_Tick(float dt) {
  if (!s_InMode) return;

  // Hotkeys (only respond while in Sequence mode)
  if (IsKeyJustUp(g_SeqHotkeyAdd))  { Sequence_CapturePoseAtCurrentTime(); }
  if (IsKeyJustUp(g_SeqHotkeyPlay)) { Sequence_TogglePlay(); }
  if (IsKeyJustUp(g_SeqHotkeyStop)) { Sequence_Stop(); }
  if (IsKeyJustUp(g_SeqHotkeyNext)) { Sequence_JumpToNextPose(); }

  // Drive playback if running. When NOT playing we leave the camera
  // alone so the user can free-fly via UpdateFreeCamera (the dispatcher
  // in script.cpp calls it for us). Scrub preview is applied on demand
  // by Sequence_SetCurrentTime, not every frame.
  if (s_Playing) {
    CameraSequence *s = Sequence_Active();
    if (!s || s->poses.empty()) {
      Sequence_Stop();
      return;
    }
    float speed = s->playbackSpeed > 0.0f ? s->playbackSpeed : 1.0f;
    bool firstTick = s_FirstPlayTick;
    s_FirstPlayTick = false;
    bool wrapped = false;
    s_PlaybackTime += dt * speed;
    float dur = Sequence_TotalDuration();
    if (s_PlaybackTime >= dur) {
      if (s->loop && dur > 0.0001f) {
        s_PlaybackTime = fmodf(s_PlaybackTime, dur);
        s_LastTickTime = 0.0f;
        wrapped = true;
      } else {
        s_PlaybackTime = dur;
        s_Playing = false;
        g_SequencePlaybackActive = false;
        // End of non-looped playback — restore the pre-play shake state
        // so the camera doesn't keep shaking forever in authoring mode.
        RestoreShakeSnapshot();
      }
    }
    ApplyPoseAtTime(s_PlaybackTime, s);
    DriveEvents(s, s_LastTickTime, s_PlaybackTime, firstTick || wrapped);
    s_LastTickTime = s_PlaybackTime;
    SequencePushToEngine(dt);
  }

  // Always draw keyframe markers + path preview while in Sequence mode
  // (dimmed during playback so they don't dominate the shot).
  DrawSequenceMarkers();
}

// ============================================================
//  Persistence — SimpleCamera_Sequences.ini
// ============================================================

static void GetSequencesIniPath(char *outPath, size_t bufSize) {
  HMODULE hMod = NULL;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCSTR)GetSequencesIniPath, &hMod);
  GetModuleFileNameA(hMod, outPath, (DWORD)bufSize);
  char *last = strrchr(outPath, '\\');
  if (last)
    strcpy_s(last + 1, bufSize - (last + 1 - outPath),
             "SimpleCamera_Sequences.ini");
}

static void ParsePose(const char *line, PoseKeyframe &p) {
  // Format: t=X,x=X,y=X,z=X,pitch=X,yaw=X,roll=X,fov=X,ease=N,path=N
  p = PoseKeyframe{};
  p.ease = EASE_LINEAR;
  p.path = PATH_LINEAR;
  char buf[512]; strncpy_s(buf, sizeof(buf), line, _TRUNCATE);
  char *ctx = nullptr;
  for (char *tok = strtok_s(buf, ",", &ctx); tok; tok = strtok_s(nullptr, ",", &ctx)) {
    char *eq = strchr(tok, '=');
    if (!eq) continue;
    *eq = '\0';
    const char *k = tok;
    const char *v = eq + 1;
    if      (!strcmp(k, "t"))     p.t = (float)atof(v);
    else if (!strcmp(k, "x"))     p.posX = (float)atof(v);
    else if (!strcmp(k, "y"))     p.posY = (float)atof(v);
    else if (!strcmp(k, "z"))     p.posZ = (float)atof(v);
    else if (!strcmp(k, "pitch")) p.pitch = (float)atof(v);
    else if (!strcmp(k, "yaw"))   p.yaw = (float)atof(v);
    else if (!strcmp(k, "roll"))  p.roll = (float)atof(v);
    else if (!strcmp(k, "fov"))   p.fov = (float)atof(v);
    else if (!strcmp(k, "ease"))  p.ease = (EaseType)atoi(v);
    else if (!strcmp(k, "path"))  p.path = (PathType)atoi(v);
  }
}

static void ParseEvent(const char *line, EffectEvent &e) {
  e = EffectEvent{};
  char buf[256]; strncpy_s(buf, sizeof(buf), line, _TRUNCATE);
  char *ctx = nullptr;
  for (char *tok = strtok_s(buf, ",", &ctx); tok; tok = strtok_s(nullptr, ",", &ctx)) {
    char *eq = strchr(tok, '=');
    if (!eq) continue;
    *eq = '\0';
    const char *k = tok;
    const char *v = eq + 1;
    if      (!strcmp(k, "t"))     e.t = (float)atof(v);
    else if (!strcmp(k, "kind"))  e.kind = (EffectKind)atoi(v);
    else if (!strcmp(k, "value")) e.value = (float)atof(v);
    else if (!strcmp(k, "ramp"))  e.ramp = atoi(v) != 0;
  }
}

static void FormatPose(const PoseKeyframe &p, char *out, size_t bufSize) {
  sprintf_s(out, bufSize,
            "t=%.3f,x=%.3f,y=%.3f,z=%.3f,pitch=%.3f,yaw=%.3f,roll=%.3f,fov=%.3f,ease=%d,path=%d",
            p.t, p.posX, p.posY, p.posZ, p.pitch, p.yaw, p.roll, p.fov,
            (int)p.ease, (int)p.path);
}

static void FormatEvent(const EffectEvent &e, char *out, size_t bufSize) {
  sprintf_s(out, bufSize, "t=%.3f,kind=%d,value=%.4f,ramp=%d", e.t,
            (int)e.kind, e.value, e.ramp ? 1 : 0);
}

void Sequence_LoadAll() {
  char path[MAX_PATH];
  GetSequencesIniPath(path, sizeof(path));

  s_Sequences.clear();
  s_ActiveIdx = -1;

  // Enumerate section names by reading the whole INI's names buffer
  char names[8192] = {0};
  DWORD got = GetPrivateProfileSectionNamesA(names, sizeof(names), path);
  if (got == 0) {
    EnsureActive();
    return;
  }

  for (const char *p = names; *p; p += strlen(p) + 1) {
    if (strncmp(p, "Sequence:", 9) != 0) continue;
    CameraSequence seq;
    seq.name = p + 9;
    seq.loop = GetPrivateProfileIntA(p, "Loop", 0, path) != 0;
    char speedBuf[32];
    GetPrivateProfileStringA(p, "Speed", "1.0", speedBuf, sizeof(speedBuf), path);
    seq.playbackSpeed = (float)atof(speedBuf);

    int poseCount = GetPrivateProfileIntA(p, "PoseCount", 0, path);
    for (int i = 0; i < poseCount; ++i) {
      char key[32]; sprintf_s(key, "Pose%d", i);
      char buf[512];
      GetPrivateProfileStringA(p, key, "", buf, sizeof(buf), path);
      if (!*buf) continue;
      PoseKeyframe pk;
      ParsePose(buf, pk);
      seq.poses.push_back(pk);
    }
    int eventCount = GetPrivateProfileIntA(p, "EventCount", 0, path);
    for (int i = 0; i < eventCount; ++i) {
      char key[32]; sprintf_s(key, "Event%d", i);
      char buf[256];
      GetPrivateProfileStringA(p, key, "", buf, sizeof(buf), path);
      if (!*buf) continue;
      EffectEvent ev;
      ParseEvent(buf, ev);
      seq.events.push_back(ev);
    }
    s_Sequences.push_back(seq);
  }

  if (s_Sequences.empty()) EnsureActive();
  else s_ActiveIdx = 0;
}

void Sequence_SaveAll() {
  char path[MAX_PATH];
  GetSequencesIniPath(path, sizeof(path));

  // First wipe any existing Sequence:* sections so deletions are honored.
  char names[8192] = {0};
  GetPrivateProfileSectionNamesA(names, sizeof(names), path);
  for (const char *p = names; *p; p += strlen(p) + 1) {
    if (strncmp(p, "Sequence:", 9) == 0) {
      WritePrivateProfileStringA(p, nullptr, nullptr, path);
    }
  }

  for (const auto &s : s_Sequences) {
    std::string section = "Sequence:" + s.name;
    char buf[64];
    sprintf_s(buf, "%d", s.loop ? 1 : 0);
    WritePrivateProfileStringA(section.c_str(), "Loop", buf, path);
    sprintf_s(buf, "%.4f", s.playbackSpeed);
    WritePrivateProfileStringA(section.c_str(), "Speed", buf, path);
    sprintf_s(buf, "%d", (int)s.poses.size());
    WritePrivateProfileStringA(section.c_str(), "PoseCount", buf, path);
    for (int i = 0; i < (int)s.poses.size(); ++i) {
      char key[32]; sprintf_s(key, "Pose%d", i);
      char val[512];
      FormatPose(s.poses[i], val, sizeof(val));
      WritePrivateProfileStringA(section.c_str(), key, val, path);
    }
    sprintf_s(buf, "%d", (int)s.events.size());
    WritePrivateProfileStringA(section.c_str(), "EventCount", buf, path);
    for (int i = 0; i < (int)s.events.size(); ++i) {
      char key[32]; sprintf_s(key, "Event%d", i);
      char val[256];
      FormatEvent(s.events[i], val, sizeof(val));
      WritePrivateProfileStringA(section.c_str(), key, val, path);
    }
  }
}
