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
extern bool g_FreezeWorld;
extern bool g_ShowInfoOverlay;
extern bool g_ShowLockedEntityMarker;

extern bool g_IsFiveM;

// ---- Mode dispatcher ----
// 0 = Free Camera, 1 = Camera Sequence, -1 = no mode chosen yet (show picker)
extern int g_CameraMode;
extern bool g_SequencePlaybackActive;

// Write the camera transform from sequence playback. Skips input processing,
// collision, follow — these are handled by the sequence's own driver.
void SetCameraStateFromSequence(float posX, float posY, float posZ,
                                float pitch, float yaw, float roll, float fov);

// Push the current s_Pos* / s_Pitch / s_Yaw / g_CamRoll / g_CamFOV state to
// the scripted camera (CAM::SET_CAM_COORD / SET_CAM_ROT / FOV). Called by
// the sequence tick after writing state. `dt` is the per-frame delta in
// seconds — needed so procedural shake (when enabled via an effect event)
// can advance its phase. Pass 0 to suppress shake offsets entirely.
void SequencePushToEngine(float dt);

// ---- Functions ----

void DetectFiveM();
void InitFreeCamera();
void DestroyFreeCamera();
void UpdateFreeCamera();
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
