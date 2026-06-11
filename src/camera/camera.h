/*
        GTA V Free Camera / Photo Mode Plugin
        Camera System Header
*/

#pragma once

#include "external\scripthook_sdk\inc\enums.h"
#include "external\scripthook_sdk\inc\main.h"
#include "external\scripthook_sdk\inc\natives.h"
#include "external\scripthook_sdk\inc\types.h"

// ---- Camera State ----

extern bool g_FreeCamActive;
extern float g_CamSpeed;
extern float g_CamSensitivity;
extern float g_CamFOV;
extern float g_CamRoll;
extern float g_RollSpeed;
extern float g_ZoomSpeed;
extern bool g_MovePlayerWithCamera;
extern bool g_LockCamera;
extern bool g_EnablePlayerMovement;
extern bool g_CamCollision;
extern bool g_LockHeight;
extern bool g_RotationEngine; // false = Euler, true = Quaternion

// ---- Follow Entity Mode ----

extern int g_FollowMode;         // 0 = None, 1 = Player, 2 = Aimed Entity
extern int g_FollowTargetEntity; // Entity handle to follow
extern bool g_FollowRigidMode;   // Track target entity rotation

// ---- Drone Camera Mode ----

extern bool g_DroneMode;
extern float g_DroneDrag;         // velocity damping (higher = more drag)
extern float g_DroneAcceleration; // how quickly the camera accelerates
extern float g_DroneGravity;      // downward pull when not thrusting
extern float g_DroneBanking;      // auto-roll into turns (degrees)
extern float g_DroneRotSmoothing; // rotational inertia (higher = snappier)
extern float g_DroneFovSmoothing; // smoothing for FOV zoom in drone mode

// ---- Procedural Camera Shake ----

extern bool g_ShakeEnabled;
extern int g_ShakePreset;            // 0=Off,1=Subtle,2=Handheld,3=Vehicle,4=Earthquake,5=Custom
extern float g_ShakeAmp;             // base amplitude
extern float g_ShakeFreq;            // base frequency (Hz)
extern float g_ShakeSpeedAmpCoupling;  // 0..2
extern float g_ShakeSpeedFreqCoupling; // 0..2
extern float g_ShakeRotWeight;       // 0..2 — rotation contribution
extern float g_ShakePosWeight;       // 0..2 — translation contribution
extern float g_ShakeSpeedRefMax;     // reference max speed (m/s) for normalization
extern bool g_ShakeStopWhenStill;    // fade shake out when camera not translating

// ---- Walk Mode (terrain-following at fixed eye height) ----

extern bool g_WalkMode;
extern float g_WalkHeight; // meters above ground

// ---- Auto Drive ----
// AI drives the player's CURRENT vehicle to the map waypoint (or wanders the
// roads) while the free camera flies independently — turning the player's car
// into a self-driving subject you can film hands-free. Land vehicles only.

extern bool g_AutoDriveEnabled;
extern int g_AutoDriveMode;       // 0 = Go To Waypoint, 1 = Drive Anywhere (wander)
extern float g_AutoDriveSpeed;    // target speed in m/s (menu shows km/h)
extern int g_AutoDriveStyleIndex; // index into the driving-style table

extern const char *g_AutoDriveStyleNames[];
extern const int g_AutoDriveStyleCount;
int AutoDriveStyleValue(int index); // bitflag for the indexed style

void UpdateAutoDrive(); // per-frame tick (main loop + while a menu is open)
void AutoDrive_Stop();  // disable + clear the AI driving task

void ApplyShakePreset(int preset);
void RandomizeShakePattern();

// ---- Depth of Field Settings ----

extern bool g_DoFEnabled;
extern bool g_DoFAutofocus;
extern float g_DoFFocusDist;
extern float g_DoFMaxNearInFocus;
extern float g_DoFMaxFarInFocus;

// ---- Time & Weather ----

extern bool g_TimePaused;
extern int g_TimeHour;
extern int g_TimeMinute;
extern int g_Weather1Index;
extern int g_Weather2Index;
extern float g_WeatherBlend;
extern bool g_BlendWeatherActive;
extern int g_TimelapseMode;

extern const char *g_WeatherNames[];
extern const int g_WeatherCount;

// ---- Misc ----

extern bool g_HideHUD;
extern bool g_DisableVehicleShake;
extern bool g_HidePlayer;
extern bool g_RememberCamPosition;
extern bool g_FreezeWorld;     // Pause Game (SET_GAME_PAUSED) — full halt
extern bool g_FreezeEntities;  // Freeze All Entities — camera/audio stay live
extern float g_WorldTimeScale; // slow-motion 0.01..1.0 (1.0 = real time)
extern bool g_ShowInfoOverlay;
extern bool g_ShowLockedEntityMarker;

extern bool g_IsFiveM;

// True while a configuration menu is open (ProcessConfigMenu running). The
// camera keeps updating so you can compose with the menu up, but input that
// the menu also consumes (controller D-Pad = zoom vs. menu navigation) is
// suppressed in UpdateFreeCamera to avoid double-acting.
extern bool g_MenuOpen;

// ---- Mode dispatcher ----
// 0 = Free Camera, 1 = Camera Sequence, -1 = no mode chosen yet (show picker)
extern int g_CameraMode;
extern bool g_SequencePlaybackActive;

// Write the camera transform from sequence playback. Skips input processing,
// collision, follow — these are handled by the sequence's own driver.
void SetCameraStateFromSequence(float posX, float posY, float posZ,
                                float pitch, float yaw, float roll, float fov);

// Locked-segment variant of SetCameraStateFromSequence. When the current
// playback segment is anchored to a single entity (both endpoint
// keyframes locked to the same handle), the sequence driver computes
// the camera's position as an entity-local offset and hands it here.
// SequencePushToEngine then binds the camera to the entity via
// ATTACH_CAM_TO_ENTITY instead of writing world coords each frame —
// eliminating the one-frame lag inherent in script-driven SET_CAM_COORD
// on a moving entity (the same trick free-cam's rigid follow uses).
//
// The world coord is also written so shake / world-only fallback have
// a fresh value; the attachment overrides at render time.
void SetCameraStateFromSequenceLocked(int entity, float localOffsetX,
                                      float localOffsetY, float localOffsetZ,
                                      float pitch, float yaw, float roll,
                                      float fov);

// Push the current s_Pos* / s_Pitch / s_Yaw / g_CamRoll / g_CamFOV state to
// the scripted camera (CAM::SET_CAM_COORD / SET_CAM_ROT / FOV). Called by
// the sequence tick after writing state. `dt` is the per-frame delta in
// seconds — needed so procedural shake (when enabled via an effect event)
// can advance its phase. Pass 0 to suppress shake offsets entirely.
void SequencePushToEngine(float dt);

// Detach the scripted camera from any entity a locked sequence segment
// attached it to. Call when playback stops so free-fly authoring can move the
// camera again (an attached cam ignores SET_CAM_COORD — rotation-only).
void SequenceDetachCamera();

// ---- Functions ----

void DetectFiveM();
void InitFreeCamera();
void DestroyFreeCamera();
void UpdateFreeCamera();

// Quick camera actions (Misc menu)
void SnapCameraToPlayer();
void LevelCameraHorizon();
void UpdateTimeWeather();
void UpdateGlobalEffects();
void DrawInfoOverlay();

// Native wrappers for compatibility
void SetClockTime(int hour, int minute, int second);
void SetWeatherTransition(Hash w1, Hash w2, float blend);

// IGCS Connector support
void IGCS_ResetOffsets();
void IGCS_ShiftCamera(float rightStep, float upStep, float forwardStep,
                      float pitchDelta, float yawDelta, float rollDelta);
void IGCS_SetCameraPosition(float posX, float posY, float posZ, float pitch,
                            float yaw, float roll);
void GetCameraState(float &posX, float &posY, float &posZ, float &pitch,
                    float &yaw, float &roll);

// ---- PAD Shim ----
namespace PAD {
static inline void DISABLE_ALL_CONTROL_ACTIONS(int padIndex) {
  invoke<Void>(0x5F4B6931816E599B, padIndex);
}
static inline void ENABLE_CONTROL_ACTION(int padIndex, int control,
                                         BOOL enable) {
  invoke<Void>(0x351220255D64C155, padIndex, control, enable);
}
static inline float GET_DISABLED_CONTROL_NORMAL(int padIndex, int control) {
  return invoke<float>(0x11E65974A982637C, padIndex, control);
}
static inline BOOL IS_DISABLED_CONTROL_PRESSED(int padIndex, int control) {
  return invoke<BOOL>(0xE2587F8CBBD87B1D, padIndex, control);
}
static inline BOOL IS_DISABLED_CONTROL_JUST_PRESSED(int padIndex, int control) {
  return invoke<BOOL>(0x91AEF906BCA88877, padIndex, control);
}
} // namespace PAD
