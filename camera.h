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
extern bool g_HidePlayer;
extern bool g_RememberCamPosition;
extern bool g_FreezeWorld;
extern bool g_ShowInfoOverlay;
extern bool g_ShowLockedEntityMarker;

// ---- Functions ----

void InitFreeCamera();
void DestroyFreeCamera();
void UpdateFreeCamera();
void UpdateTimeWeather();
void UpdateGlobalEffects();
void DrawInfoOverlay();

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
