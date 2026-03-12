/*
        GTA V Free Camera / Photo Mode Plugin
        Camera System Implementation

        6-DOF free camera with smooth keyboard + mouse + controller input,
        depth of field integration, and time/weather overrides.
*/

#include "camera.h"
#include "igcs_bridge.h"
#include "keyboard.h"
#include <cmath>
#include <cstdio>

#pragma warning(disable : 4244 4305)

// ============================================================
//  Constants
// ============================================================

static const float PI = 3.14159265358979323846f;
static const float DEG2RAD = PI / 180.0f;

// Controller input IDs (GTA V control enums)
static const int INPUT_LOOK_LR = 1;           // Right stick X
static const int INPUT_LOOK_UD = 2;           // Right stick Y
static const int INPUT_MOVE_LR = 30;          // Left stick X
static const int INPUT_MOVE_UD = 31;          // Left stick Y
static const int INPUT_ATTACK = 24;           // RT
static const int INPUT_AIM = 25;              // LT
static const int INPUT_FRONTEND_LB = 205;     // LB
static const int INPUT_FRONTEND_RB = 206;     // RB
static const int INPUT_FRONTEND_UP = 188;     // D-pad Up
static const int INPUT_FRONTEND_DOWN = 187;   // D-pad Down
static const int INPUT_FRONTEND_ACCEPT = 201; // A / Enter
static const int INPUT_FRONTEND_CANCEL = 202; // B / Backspace

// ============================================================
//  Global State
// ============================================================

bool g_FreeCamActive = false;
float g_CamSpeed = 1.0f;
float g_CamSensitivity = 1.0f;
float g_CamFOV = 50.0f;
float g_CamRoll = 0.0f;
float g_ZoomSpeed = 1.0f;
bool g_MovePlayerWithCamera = false;
bool g_LockCamera = false;
bool g_EnablePlayerMovement = false;
bool g_CamCollision = false;
bool g_LockHeight = false;
bool g_RotationEngine = false;

// Follow Entity Mode
int g_FollowMode = 0; // 0 = None, 1 = Player, 2 = Aimed Entity
int g_FollowTargetEntity = 0;
bool g_FollowRigidMode = false;

// Drone Camera Mode
bool g_DroneMode = false;
float g_DroneDrag = 3.0f;
float g_DroneAcceleration = 15.0f;
float g_DroneGravity = 0.0f;
float g_DroneBanking = 15.0f;
float g_DroneRotSmoothing = 8.0f;
float g_DroneFovSmoothing = 5.0f;

// Depth of Field
bool g_DoFEnabled = false;
bool g_DoFAutofocus = false;
float g_DoFFocusDist = 10.0f;
float g_DoFMaxNearInFocus = 0.5f;
float g_DoFMaxFarInFocus = 5.0f;

// Time & Weather
bool g_TimePaused = false;
int g_TimeHour = 12;
int g_TimeMinute = 0;
int g_Weather1Index = 0;
int g_Weather2Index = 1;
float g_WeatherBlend = 0.0f;
bool g_BlendWeatherActive = false;
int g_TimelapseMode = 0;

const char *g_WeatherNames[] = {
    "CLEAR",     "EXTRASUNNY", "CLOUDS",  "OVERCAST", "RAIN",
    "CLEARING",  "THUNDER",    "SMOG",    "FOGGY",    "XMAS",
    "SNOWLIGHT", "BLIZZARD",   "NEUTRAL", "SNOW"};
const int g_WeatherCount = sizeof(g_WeatherNames) / sizeof(g_WeatherNames[0]);

// ---- Misc ----
bool g_HideHUD = false;
bool g_HidePlayer = false;
bool g_RememberCamPosition = false;
bool g_FreezeWorld = false;
bool g_ShowInfoOverlay = false;
bool g_ShowLockedEntityMarker = true;

// ---- Internal camera state ----

static Cam s_Cam = 0;
static float s_PosX = 0.0f, s_PosY = 0.0f, s_PosZ = 0.0f;
static float s_Pitch = 0.0f, s_Yaw = 0.0f; // degrees (heading)
static float s_IgcsOffsetX = 0.0f, s_IgcsOffsetY = 0.0f, s_IgcsOffsetZ = 0.0f;
static float s_IgcsPitchOffset = 0.0f, s_IgcsYawOffset = 0.0f,
             s_IgcsRollOffset = 0.0f;
static Ped s_FrozenPed = 0;
static bool s_HasSavedPosition = false;
static Entity s_RotationAnchor = 0;

// Drone physics state
static float s_DroneVelX = 0.0f;
static float s_DroneVelY = 0.0f;
static float s_DroneVelZ = 0.0f;
static float s_DroneYawRate = 0.0f;   // degrees/sec
static float s_DronePitchRate = 0.0f; // degrees/sec
static float s_DroneRollRate = 0.0f;  // degrees/sec
static float s_DroneTargetRoll = 0.0f;

// ============================================================
//  Quaternion Support for Acrobatic Mode
// ============================================================

struct Quat {
  float w, x, y, z;
};

static Quat MultiplyQuat(const Quat &q1, const Quat &q2) {
  return {q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
          q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
          q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
          q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w};
}

static Quat AxisAngleToQuat(float axisX, float axisY, float axisZ,
                            float angleRad) {
  float halfAngle = angleRad * 0.5f;
  float s = sinf(halfAngle);
  return {cosf(halfAngle), axisX * s, axisY * s, axisZ * s};
}

static Quat NormalizeQuat(const Quat &q) {
  float len = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (len > 0.00001f) {
    return {q.w / len, q.x / len, q.y / len, q.z / len};
  }
  return {1.0f, 0.0f, 0.0f, 0.0f};
}

static float s_LastYaw = 0.0f;
static float s_LastRoll = 0.0f;

static void QuatToEuler(const Quat &q, float &pitchOut, float &yawOut,
                        float &rollOut) {
  // Convert Quaternion to Rotation Matrix elements
  float m11 = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
  float m12 = 2.0f * (q.x * q.y - q.z * q.w);
  float m13 = 2.0f * (q.x * q.z + q.y * q.w);

  float m21 = 2.0f * (q.x * q.y + q.z * q.w);
  float m22 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
  float m23 = 2.0f * (q.y * q.z - q.x * q.w);

  float m31 = 2.0f * (q.x * q.z - q.y * q.w);
  float m32 = 2.0f * (q.y * q.z + q.x * q.w);
  float m33 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

  // GTA V Rotation Order 2 (ZXY equivalent)
  pitchOut = asinf(max(-1.0f, min(1.0f, m32))) * (180.0f / PI);

  // Check for Gimbal Lock (Pitch near +/- 90)
  if (abs(m32) < 0.999f) {
    yawOut = atan2f(-m12, m22) * (180.0f / PI);
    rollOut = atan2f(-m31, m33) * (180.0f / PI);

    // Store safe orientations
    s_LastYaw = yawOut;
    s_LastRoll = rollOut;
  } else {

    yawOut = atan2f(m21, m11) * (180.0f / PI);

    if (m32 < 0) {
      yawOut -= s_LastRoll;
    } else {
      yawOut += s_LastRoll;
    }

    // Smooth boundary clamp
    // Restricting Roll manually prevents the "Screen Flip" structural snap
    rollOut = s_LastRoll;
  }
}

static Quat EulerToQuat(float pitchDeg, float yawDeg, float rollDeg) {
  Quat qYaw = AxisAngleToQuat(0, 0, 1, -yawDeg * DEG2RAD);
  Quat qPitch = AxisAngleToQuat(1, 0, 0, pitchDeg * DEG2RAD);
  Quat qRoll = AxisAngleToQuat(0, 1, 0, rollDeg * DEG2RAD);

  Quat qCombined = MultiplyQuat(qYaw, qPitch);
  return MultiplyQuat(qCombined, qRoll);
}

static Quat s_AcroMatrix = {1.0f, 0.0f, 0.0f, 0.0f};

// ============================================================
//  Init / Destroy
// ============================================================

void InitFreeCamera() {
  if (g_FreeCamActive)
    return;

  // If we have a saved position and the user wants to remember it, reuse it
  if (!(g_RememberCamPosition && s_HasSavedPosition)) {
    // Grab current gameplay camera state as starting point
    Vector3 gcPos = CAM::GET_GAMEPLAY_CAM_COORD();
    Vector3 gcRot = CAM::GET_GAMEPLAY_CAM_ROT(2);

    s_PosX = gcPos.x;
    s_PosY = gcPos.y;
    s_PosZ = gcPos.z;
    s_Pitch = gcRot.x;
    s_Yaw = gcRot.z;
    g_CamRoll = 0.0f;
    g_CamFOV = CAM::GET_GAMEPLAY_CAM_FOV();

    // Initialize the Acro quaternion based on the Gameplay Cam Euler memory
    s_AcroMatrix = EulerToQuat(s_Pitch, s_Yaw, g_CamRoll);
  } else {
    // If remembering previous position, sync the AcroMatrix to our saved Eulers
    s_AcroMatrix = EulerToQuat(s_Pitch, s_Yaw, g_CamRoll);
  }

  // Create the scripted camera
  s_Cam = CAM::CREATE_CAM((char *)"DEFAULT_SCRIPTED_CAMERA", TRUE);
  CAM::SET_CAM_COORD(s_Cam, s_PosX, s_PosY, s_PosZ);
  CAM::SET_CAM_ROT(s_Cam, s_Pitch, g_CamRoll, s_Yaw, 2);
  CAM::SET_CAM_FOV(s_Cam, g_CamFOV);
  CAM::SET_CAM_ACTIVE(s_Cam, TRUE);
  CAM::RENDER_SCRIPT_CAMS(TRUE, FALSE, 0, TRUE, FALSE);

  // Freeze the player ped (unless player movement is enabled)
  Ped playerPed = PLAYER::PLAYER_PED_ID();
  s_FrozenPed = playerPed;
  if (!g_EnablePlayerMovement) {
    ENTITY::FREEZE_ENTITY_POSITION(playerPed, TRUE);
    ENTITY::SET_ENTITY_COLLISION(playerPed, FALSE, FALSE);
  }

  // Auto-hide HUD and Player when entering freecam
  g_HideHUD = true;
  g_HidePlayer = true;

  g_FreeCamActive = true;
}

void DestroyFreeCamera() {
  if (!g_FreeCamActive)
    return;

  // Restore player
  if (ENTITY::DOES_ENTITY_EXIST(s_FrozenPed)) {
    ENTITY::FREEZE_ENTITY_POSITION(s_FrozenPed, FALSE);
    ENTITY::SET_ENTITY_COLLISION(s_FrozenPed, TRUE, FALSE);
  }

  // Auto-disable HUD/Player hiding when exiting freecam
  g_HideHUD = false;
  g_HidePlayer = false;

  // Destroy script camera and restore gameplay camera
  CAM::RENDER_SCRIPT_CAMS(FALSE, FALSE, 0, TRUE, FALSE);
  CAM::SET_CAM_ACTIVE(s_Cam, FALSE);
  CAM::DESTROY_CAM(s_Cam, FALSE);
  s_Cam = 0;

  // Mark that we now have a saved position for next time
  s_HasSavedPosition = true;

  // Restore time scale if frozen
  if (g_FreezeWorld) {
    GAMEPLAY::SET_TIME_SCALE(1.0f);
    g_FreezeWorld = false;
  }

  // Reset lock/movement flags
  g_LockCamera = false;
  g_EnablePlayerMovement = false;
  g_CamCollision = false;

  g_FreeCamActive = false;
}

// ============================================================
//  Info Overlay Drawing
// ============================================================

void DrawOverlayLine(const char *text, float y) {
  UI::SET_TEXT_FONT(0);
  UI::SET_TEXT_SCALE(0.0f, 0.30f);
  UI::SET_TEXT_COLOUR(220, 220, 220, 220);
  UI::SET_TEXT_CENTRE(0);
  UI::SET_TEXT_DROPSHADOW(2, 0, 0, 0, 200);
  UI::SET_TEXT_EDGE(1, 0, 0, 0, 200);
  UI::SET_TEXT_RIGHT_JUSTIFY(1);
  UI::SET_TEXT_WRAP(0.0f, 0.995f);
  UI::_SET_TEXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)text);
  UI::_DRAW_TEXT(0.995f, y);
}

void DrawInfoOverlay() {
  char buf[128];
  float y = 0.005f;
  float lineH = 0.022f;

  // Show camera position if freecam is active, otherwise show player position
  if (g_FreeCamActive) {
    snprintf(buf, sizeof(buf), "Pos: %.2f, %.2f, %.2f", s_PosX, s_PosY, s_PosZ);
  } else {
    Ped playerPed = PLAYER::PLAYER_PED_ID();
    Vector3 pPos = ENTITY::GET_ENTITY_COORDS(playerPed, TRUE);
    snprintf(buf, sizeof(buf), "Pos: %.2f, %.2f, %.2f", pPos.x, pPos.y, pPos.z);
  }
  DrawOverlayLine(buf, y);
  y += lineH;

  snprintf(buf, sizeof(buf), "Rot: P %.1f  Y %.1f  R %.1f", s_Pitch, s_Yaw,
           g_CamRoll);
  DrawOverlayLine(buf, y);
  y += lineH;

  snprintf(buf, sizeof(buf), "FOV: %.1f   Speed: %.1f", g_CamFOV, g_CamSpeed);
  DrawOverlayLine(buf, y);
  y += lineH;

  if (g_DoFEnabled) {
    snprintf(buf, sizeof(buf), "DoF: %.1f  Near: %.1f  Far: %.1f",
             g_DoFFocusDist, g_DoFMaxNearInFocus, g_DoFMaxFarInFocus);
    DrawOverlayLine(buf, y);
    y += lineH;
  }

  if (g_FreezeWorld) {
    DrawOverlayLine("~b~WORLD FROZEN", y);
    y += lineH;
  }
}

// ============================================================
//  Per-Frame Update
// ============================================================

void UpdateFreeCamera() {
  if (!g_FreeCamActive)
    return;

  // Use real-time delta when world is frozen, since GET_FRAME_TIME returns 0
  static DWORD s_LastFrameTick = 0;
  DWORD now = GetTickCount();
  float dt;
  if (g_FreezeWorld) {
    dt = (s_LastFrameTick > 0) ? (now - s_LastFrameTick) / 1000.0f : 0.016f;
  } else {
    dt = GAMEPLAY::GET_FRAME_TIME();
  }
  s_LastFrameTick = now;
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.016f;

  // --- Follow Entity Delta Tracking ---
  static Vector3 s_LastTargetPos = {0, 0, 0};
  static Vector3 s_LastTargetRot = {0, 0, 0};
  static bool s_IsAttached = false;
  static int s_AttachedEntity = 0;
  static Vector3 s_LocalOffset = {0, 0, 0};

  int targetHandle = 0;

  if (g_FollowMode == 1) {
    targetHandle = PLAYER::PLAYER_PED_ID();
  } else if (g_FollowMode == 2 && g_FollowTargetEntity != 0) {
    targetHandle = g_FollowTargetEntity;
  }

  if (targetHandle != 0 && ENTITY::DOES_ENTITY_EXIST(targetHandle)) {
    Vector3 curTargetPos = ENTITY::GET_ENTITY_COORDS(targetHandle, TRUE);
    Vector3 curTargetRot = ENTITY::GET_ENTITY_ROTATION(targetHandle, 2);

    if (g_FollowRigidMode) {
      if (!s_IsAttached || s_AttachedEntity != targetHandle) {
        s_LocalOffset = invoke<Vector3>(
            0x2274BC1C4885E333, targetHandle, s_PosX, s_PosY,
            s_PosZ); // GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS
        invoke<Void>(0xFEDB7D269E8C60E3, s_Cam, targetHandle, s_LocalOffset.x,
                     s_LocalOffset.y, s_LocalOffset.z,
                     TRUE); // ATTACH_CAM_TO_ENTITY
        s_IsAttached = true;
        s_AttachedEntity = targetHandle;
      }

      // Sync script variables to the current attached world position so flying
      // and collisions work natively
      Vector3 attachedPos = invoke<Vector3>(
          0x1899F328B0E12848, targetHandle, s_LocalOffset.x, s_LocalOffset.y,
          s_LocalOffset.z); // GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS
      s_PosX = attachedPos.x;
      s_PosY = attachedPos.y;
      s_PosZ = attachedPos.z;

      if (s_LastTargetRot.x != 0 || s_LastTargetRot.y != 0 ||
          s_LastTargetRot.z != 0) {
        float dPitch = curTargetRot.x - s_LastTargetRot.x;
        float dRoll = curTargetRot.y - s_LastTargetRot.y;
        float dYaw = curTargetRot.z - s_LastTargetRot.z;

        while (dPitch > 180.0f)
          dPitch -= 360.0f;
        while (dPitch <= -180.0f)
          dPitch += 360.0f;

        while (dRoll > 180.0f)
          dRoll -= 360.0f;
        while (dRoll <= -180.0f)
          dRoll += 360.0f;

        while (dYaw > 180.0f)
          dYaw -= 360.0f;
        while (dYaw <= -180.0f)
          dYaw += 360.0f;

        // Apply rotation deltas
        s_Pitch += dPitch;
        g_CamRoll += dRoll;
        s_Yaw += dYaw;
      }
    } else {
      if (s_IsAttached) {
        invoke<Void>(0xA2FABBE87F4BAD82, s_Cam, FALSE); // DETACH_CAM
        s_IsAttached = false;
        s_AttachedEntity = 0;
        CAM::SET_CAM_COORD(s_Cam, s_PosX, s_PosY, s_PosZ);
      }

      // If we already have a valid last pos, apply the delta
      if (s_LastTargetPos.x != 0 || s_LastTargetPos.y != 0 ||
          s_LastTargetPos.z != 0) {
        s_PosX += (curTargetPos.x - s_LastTargetPos.x);
        s_PosY += (curTargetPos.y - s_LastTargetPos.y);
        s_PosZ += (curTargetPos.z - s_LastTargetPos.z);
      }
    }

    s_LastTargetPos = curTargetPos;
    s_LastTargetRot = curTargetRot;

    if (g_ShowLockedEntityMarker && g_FollowMode == 2) {
      GRAPHICS::DRAW_MARKER(0, curTargetPos.x, curTargetPos.y,
                            curTargetPos.z + 1.25f, 0, 0, 0, 0, 0, 0, 0.4f,
                            0.4f, 0.4f, 0, 255, 0, 200, TRUE, TRUE, 2, FALSE,
                            NULL, NULL, FALSE);
    }
  } else {
    s_LastTargetPos = {0, 0, 0};
    s_LastTargetRot = {0, 0, 0};
    if (s_IsAttached) {
      invoke<Void>(0xA2FABBE87F4BAD82, s_Cam, FALSE); // DETACH_CAM
      s_IsAttached = false;
      s_AttachedEntity = 0;
      CAM::SET_CAM_COORD(s_Cam, s_PosX, s_PosY, s_PosZ);
    }
  }

  // Always disable character wheel (interferes with FOV D-pad controls)
  CONTROLS::DISABLE_CONTROL_ACTION(0, 19, TRUE);  // INPUT_CHARACTER_WHEEL
  CONTROLS::DISABLE_CONTROL_ACTION(0, 166, TRUE); // INPUT_SELECT_CHARACTER_MICHAEL
  CONTROLS::DISABLE_CONTROL_ACTION(0, 167, TRUE); // INPUT_SELECT_CHARACTER_FRANKLIN
  CONTROLS::DISABLE_CONTROL_ACTION(0, 168, TRUE); // INPUT_SELECT_CHARACTER_TREVOR
  CONTROLS::DISABLE_CONTROL_ACTION(0, 169, TRUE); // INPUT_SELECT_CHARACTER_MULTIPLAYER

  if (!g_EnablePlayerMovement) {

    // Player Movement & Actions
    CONTROLS::DISABLE_CONTROL_ACTION(0, 30, TRUE); // INPUT_MOVE_LR
    CONTROLS::DISABLE_CONTROL_ACTION(0, 31, TRUE); // INPUT_MOVE_UD
    CONTROLS::DISABLE_CONTROL_ACTION(0, 32, TRUE); // INPUT_MOVE_UP_ONLY
    CONTROLS::DISABLE_CONTROL_ACTION(0, 33, TRUE); // INPUT_MOVE_DOWN_ONLY
    CONTROLS::DISABLE_CONTROL_ACTION(0, 34, TRUE); // INPUT_MOVE_LEFT_ONLY
    CONTROLS::DISABLE_CONTROL_ACTION(0, 35, TRUE); // INPUT_MOVE_RIGHT_ONLY
    CONTROLS::DISABLE_CONTROL_ACTION(0, 21, TRUE); // INPUT_SPRINT
    CONTROLS::DISABLE_CONTROL_ACTION(0, 22, TRUE); // INPUT_JUMP
    CONTROLS::DISABLE_CONTROL_ACTION(0, 36, TRUE); // INPUT_DUCK

    // Look / Aim
    CONTROLS::DISABLE_CONTROL_ACTION(0, 1, TRUE);  // INPUT_LOOK_LR
    CONTROLS::DISABLE_CONTROL_ACTION(0, 2, TRUE);  // INPUT_LOOK_UD
    CONTROLS::DISABLE_CONTROL_ACTION(0, 25, TRUE); // INPUT_AIM
    CONTROLS::DISABLE_CONTROL_ACTION(0, 26, TRUE); // INPUT_LOOK_BEHIND

    // Phone (prevent scroll/up from opening phone)
    CONTROLS::DISABLE_CONTROL_ACTION(0, 27, TRUE);  // INPUT_PHONE
    CONTROLS::DISABLE_CONTROL_ACTION(0, 172, TRUE); // INPUT_PHONE_UP
    CONTROLS::DISABLE_CONTROL_ACTION(0, 173, TRUE); // INPUT_PHONE_DOWN
    CONTROLS::DISABLE_CONTROL_ACTION(0, 174, TRUE); // INPUT_PHONE_LEFT
    CONTROLS::DISABLE_CONTROL_ACTION(0, 175, TRUE); // INPUT_PHONE_RIGHT

    // Combat & Weapons
    CONTROLS::DISABLE_CONTROL_ACTION(0, 24, TRUE);  // INPUT_ATTACK
    CONTROLS::DISABLE_CONTROL_ACTION(0, 257, TRUE); // INPUT_ATTACK2
    CONTROLS::DISABLE_CONTROL_ACTION(0, 140, TRUE); // INPUT_MELEE_ATTACK_LIGHT
    CONTROLS::DISABLE_CONTROL_ACTION(0, 141, TRUE); // INPUT_MELEE_ATTACK_HEAVY
    CONTROLS::DISABLE_CONTROL_ACTION(0, 142,
                                     TRUE); // INPUT_MELEE_ATTACK_ALTERNATE
    CONTROLS::DISABLE_CONTROL_ACTION(0, 143, TRUE); // INPUT_MELEE_BLOCK
    CONTROLS::DISABLE_CONTROL_ACTION(0, 37, TRUE);  // INPUT_SELECT_WEAPON
    CONTROLS::DISABLE_CONTROL_ACTION(0, 14, TRUE);  // INPUT_WEAPON_WHEEL_NEXT
    CONTROLS::DISABLE_CONTROL_ACTION(0, 15, TRUE);  // INPUT_WEAPON_WHEEL_PREV
    CONTROLS::DISABLE_CONTROL_ACTION(0, 45, TRUE);  // INPUT_RELOAD
    CONTROLS::DISABLE_CONTROL_ACTION(0, 44, TRUE);  // INPUT_COVER
    CONTROLS::DISABLE_CONTROL_ACTION(0, 114, TRUE); // INPUT_VEH_FLY_ATTACK

    // Vehicle Driving
    CONTROLS::DISABLE_CONTROL_ACTION(0, 59, TRUE); // INPUT_VEH_MOVE_LR
    CONTROLS::DISABLE_CONTROL_ACTION(0, 60, TRUE); // INPUT_VEH_MOVE_UD
    CONTROLS::DISABLE_CONTROL_ACTION(0, 71, TRUE); // INPUT_VEH_ACCELERATE
    CONTROLS::DISABLE_CONTROL_ACTION(0, 72, TRUE); // INPUT_VEH_BRAKE
    CONTROLS::DISABLE_CONTROL_ACTION(0, 76, TRUE); // INPUT_VEH_HANDBRAKE
    CONTROLS::DISABLE_CONTROL_ACTION(0, 86, TRUE); // INPUT_VEH_HORN
    CONTROLS::DISABLE_CONTROL_ACTION(0, 68, TRUE); // INPUT_VEH_AIM
    CONTROLS::DISABLE_CONTROL_ACTION(0, 69, TRUE); // INPUT_VEH_ATTACK
    CONTROLS::DISABLE_CONTROL_ACTION(0, 70, TRUE); // INPUT_VEH_ATTACK2
    CONTROLS::DISABLE_CONTROL_ACTION(0, 79, TRUE); // INPUT_VEH_LOOK_BEHIND
  }

  // ---- User input (disabled when IGCS has camera locked or camera is locked)
  // ----
  if (!IGCS_IsSessionActive() && !g_LockCamera) {

    // ---- Rotation (Mouse + Right Stick) ----

    // Mouse: raw axis input (-1..1 per frame, usually small)
    float mouseX = PAD::GET_DISABLED_CONTROL_NORMAL(0, INPUT_LOOK_LR);
    float mouseY = PAD::GET_DISABLED_CONTROL_NORMAL(0, INPUT_LOOK_UD);

    float rotSpeed = g_CamSensitivity * 4.0f;

    if (g_DroneMode) {
      // Drone: smooth rotation via angular velocity interpolation
      float targetYawRate = -mouseX * rotSpeed * 60.0f; // degrees/sec
      float targetPitchRate = -mouseY * rotSpeed * 60.0f;
      float smoothFactor = 1.0f - expf(-g_DroneRotSmoothing * dt);
      s_DroneYawRate += (targetYawRate - s_DroneYawRate) * smoothFactor;
      s_DronePitchRate += (targetPitchRate - s_DronePitchRate) * smoothFactor;

      if (!g_RotationEngine) {
        s_Yaw += s_DroneYawRate * dt;
        s_Pitch += s_DronePitchRate * dt;
      }
    } else {
      if (!g_RotationEngine) {
        s_Yaw -= mouseX * rotSpeed;
        s_Pitch -= mouseY * rotSpeed;
      }
    }

    // Protect Euler angles from Gimbal Lock if Quaternion Engine is OFF
    if (!g_RotationEngine) {
      if (s_Pitch > 89.0f)
        s_Pitch = 89.0f;
      if (s_Pitch < -89.0f)
        s_Pitch = -89.0f;
    }

    // ---- Roll (Q/E keys, LB/RB controller) ----

    float rollDelta = 0.0f;
    if (IsKeyDown(0x51))
      rollDelta -= 45.0f * dt; // Q
    if (IsKeyDown(0x45))
      rollDelta += 45.0f * dt; // E

    // Controller bumpers
    if (PAD::IS_DISABLED_CONTROL_PRESSED(0, INPUT_FRONTEND_LB))
      rollDelta -= 45.0f * dt;
    if (PAD::IS_DISABLED_CONTROL_PRESSED(0, INPUT_FRONTEND_RB))
      rollDelta += 45.0f * dt;

    if (g_DroneMode && !g_RotationEngine) {
      // Auto-banking: camera tilts into yaw turns
      s_DroneTargetRoll = -s_DroneYawRate * (g_DroneBanking / 60.0f);
      // Add manual roll on top
      g_CamRoll += rollDelta;
      // Blend toward auto-bank target
      float bankBlend = 1.0f - expf(-4.0f * dt);
      g_CamRoll += (s_DroneTargetRoll - g_CamRoll) * bankBlend;
    } else {
      if (!g_RotationEngine) {
        g_CamRoll += rollDelta;
      }
    }

    // Euler Angle Roll Wrapping
    if (!g_RotationEngine) {
      if (g_CamRoll > 180.0f)
        g_CamRoll -= 360.0f;
      if (g_CamRoll < -180.0f)
        g_CamRoll += 360.0f;
    }

    // ---- Quaternion Processing ----
    if (g_RotationEngine) {
      // Calculate angular deltas
      float pitchDelta =
          g_DroneMode ? (s_DronePitchRate * dt) : (-mouseY * rotSpeed);
      float yawDelta =
          g_DroneMode ? (s_DroneYawRate * dt) : (-mouseX * rotSpeed);

      float finalRollDelta = rollDelta;

      if (g_DroneMode) {
        // Drone Roll Smoothing
        float targetRollRate = rollDelta * 60.0f; // Scale input per sec
        float smoothFactor = 1.0f - expf(-g_DroneRotSmoothing * dt);
        s_DroneRollRate += (targetRollRate - s_DroneRollRate) * smoothFactor;
        finalRollDelta = s_DroneRollRate * dt;
      }

      // Apply relative rotations to the Quaternion Matrix sequentially (Yaw,
      // Pitch, Roll)
      Quat qYaw = AxisAngleToQuat(0, 0, 1, yawDelta * DEG2RAD);
      // We pitch relative to our current matrix
      Quat qPitch = AxisAngleToQuat(1, 0, 0, pitchDelta * DEG2RAD);
      Quat qRoll = AxisAngleToQuat(
          0, 1, 0,
          finalRollDelta * DEG2RAD); // Roll around locally transformed Y

      // Update the mastery tracking Quat
      s_AcroMatrix = MultiplyQuat(s_AcroMatrix, qYaw);
      s_AcroMatrix = MultiplyQuat(s_AcroMatrix, qPitch);
      s_AcroMatrix = MultiplyQuat(s_AcroMatrix, qRoll);

      // Prevent Floating-Point Dimensional Collapse over Time
      s_AcroMatrix = NormalizeQuat(s_AcroMatrix);

      // Unpack updated Quat back to Euler angles strictly to feed the GTA
      // Engine
      QuatToEuler(s_AcroMatrix, s_Pitch, s_Yaw, g_CamRoll);
    } else {
      // Keep Quat synchronized with Euler for seamless transitioning back to
      // Acro
      s_AcroMatrix = EulerToQuat(s_Pitch, s_Yaw, g_CamRoll);
    }

    // ---- FOV / Zoom (Mouse Wheel: +/- keys, D-Pad up/down) ----

    float zoomRate = g_ZoomSpeed * 30.0f; // 1.0 = 30 deg/sec (original speed)

    // Desired zoom rate from input (deg/sec): negative = zoom in, positive = zoom out
    float targetFovRate = 0.0f;

    // Keyboard zoom
    if (IsKeyDown(VK_OEM_PLUS) || IsKeyDown(VK_ADD))
      targetFovRate -= zoomRate;
    if (IsKeyDown(VK_OEM_MINUS) || IsKeyDown(VK_SUBTRACT))
      targetFovRate += zoomRate;

    // Mouse scroll for speed (241 = scroll up, 242 = scroll down, mouse-only)
    if (PAD::IS_DISABLED_CONTROL_PRESSED(0, 241)) {
      g_CamSpeed += 10.0f * dt;
      if (g_CamSpeed > 20.0f)
        g_CamSpeed = 20.0f;
    }
    if (PAD::IS_DISABLED_CONTROL_PRESSED(0, 242)) {
      g_CamSpeed -= 10.0f * dt;
      if (g_CamSpeed < 0.1f)
        g_CamSpeed = 0.1f;
    }

    // Controller D-pad up/down for zoom
    if (PAD::IS_DISABLED_CONTROL_PRESSED(0, INPUT_FRONTEND_UP))
      targetFovRate -= zoomRate;
    if (PAD::IS_DISABLED_CONTROL_PRESSED(0, INPUT_FRONTEND_DOWN))
      targetFovRate += zoomRate;

    // Apply FOV — velocity-based inertia in drone mode, instant in standard
    static float s_DroneFovRate = 0.0f;
    if (g_DroneMode && g_DroneFovSmoothing > 0.0f) {
      // Smooth the zoom velocity (same pattern as drone rotation smoothing)
      float smoothFactor = 1.0f - expf(-g_DroneFovSmoothing * dt);
      s_DroneFovRate += (targetFovRate - s_DroneFovRate) * smoothFactor;
      g_CamFOV += s_DroneFovRate * dt;
    } else {
      g_CamFOV += targetFovRate * dt;
      s_DroneFovRate = 0.0f;
    }

    // Clamp FOV
    if (g_CamFOV < 5.0f)
      g_CamFOV = 5.0f;
    if (g_CamFOV > 130.0f)
      g_CamFOV = 130.0f;

    // ---- Translation (WASD + Space/Ctrl + Left stick + LT/RT) ----

    float moveForward = 0.0f;
    float moveRight = 0.0f;
    float moveUp = 0.0f;

    float stickX = PAD::GET_DISABLED_CONTROL_NORMAL(0, INPUT_MOVE_LR);
    float stickY = PAD::GET_DISABLED_CONTROL_NORMAL(0, INPUT_MOVE_UD);

    moveRight += stickX;
    moveForward -= stickY; // stick Y is inverted

    // Controller triggers for vertical movement
    float triggerRT = PAD::GET_DISABLED_CONTROL_NORMAL(0, INPUT_ATTACK);
    float triggerLT = PAD::GET_DISABLED_CONTROL_NORMAL(0, INPUT_AIM);

    if (moveUp == 0.0f) {
      moveUp += triggerRT;
      moveUp -= triggerLT;
    }

    // Up/Down Keyboard (Space/Ctrl)
    if (IsKeyDown(VK_SPACE))
      moveUp += 1.0f;
    if (IsKeyDown(VK_CONTROL))
      moveUp -= 1.0f;

    // Left shift = speed boost
    float speedMult = 1.0f;
    if (IsKeyDown(VK_SHIFT))
      speedMult = 3.0f;

    // Normalize movement vector so diagonal flight isn't mathematically faster
    float moveLen = sqrtf(moveForward * moveForward + moveRight * moveRight +
                          moveUp * moveUp);
    if (moveLen > 1.0f) {
      moveForward /= moveLen;
      moveRight /= moveLen;
      moveUp /= moveLen;
    }

    // Calculate world-space displacement
    float fwdX, fwdY, fwdZ;
    float rightX, rightY, rightZ;
    float upX, upY, upZ;

    if (g_RotationEngine) {
      // True 6-DOF Movement Array (Extracted directly from the Quaternion
      // Matrix)
      Quat q = s_AcroMatrix;

      // Local Forward Vector (Rotated +Y Axis)
      fwdX = 2.0f * (q.x * q.y - q.w * q.z);
      fwdY = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
      fwdZ = 2.0f * (q.y * q.z + q.w * q.x);

      // Local Right Vector (Rotated +X Axis)
      rightX = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
      rightY = 2.0f * (q.x * q.y + q.w * q.z);
      rightZ = 2.0f * (q.x * q.z - q.w * q.y);

      // Local Up Vector (Rotated +Z Axis)
      upX = 2.0f * (q.x * q.z + q.w * q.y);
      upY = 2.0f * (q.y * q.z - q.w * q.x);
      upZ = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    } else {
      // Basic 2.5D Eulerian Movement Array
      float yawRad = s_Yaw * DEG2RAD;
      float pitchRad = s_Pitch * DEG2RAD;

      float cosYaw = cosf(yawRad);
      float sinYaw = sinf(yawRad);
      float cosPitch = cosf(pitchRad);
      float sinPitch = sinf(pitchRad);

      // Forward vector (direction camera is looking)
      fwdX = -sinYaw * cosPitch;
      fwdY = cosYaw * cosPitch;
      fwdZ = sinPitch;

      // Right vector (perpendicular to forward on XY plane)
      rightX = cosYaw;
      rightY = sinYaw;
      rightZ = 0.0f;

      // Up is always world up for simplicity (no roll-based movement natively)
      upX = 0.0f;
      upY = 0.0f;
      upZ = 1.0f;
    }

    // Lock Height: Flatten the movement direction vectors to purely horizontal
    if (g_LockHeight) {
      fwdZ = 0.0f;
      float fwdLen = sqrtf(fwdX * fwdX + fwdY * fwdY);
      if (fwdLen > 0.001f) {
        fwdX /= fwdLen;
        fwdY /= fwdLen;
      }

      rightZ = 0.0f;
      float rightLen = sqrtf(rightX * rightX + rightY * rightY);
      if (rightLen > 0.001f) {
        rightX /= rightLen;
        rightY /= rightLen;
      }
    }

    if (g_DroneMode) {

      float accel = g_DroneAcceleration * g_CamSpeed * speedMult * 2.0f;

      // Acceleration from input in world space
      float accelX =
          (fwdX * moveForward + rightX * moveRight + upX * moveUp) * accel;
      float accelY =
          (fwdY * moveForward + rightY * moveRight + upY * moveUp) * accel;
      float accelZ =
          (fwdZ * moveForward + rightZ * moveRight + upZ * moveUp) * accel;

      // Apply acceleration
      s_DroneVelX += accelX * dt;
      s_DroneVelY += accelY * dt;
      s_DroneVelZ += accelZ * dt;

      // Apply gravity (only when not actively thrusting upward)
      s_DroneVelZ -= g_DroneGravity * dt;

      // Apply drag (exponential decay for smooth deceleration)
      float dragFactor = expf(-g_DroneDrag * dt);
      s_DroneVelX *= dragFactor;
      s_DroneVelY *= dragFactor;
      s_DroneVelZ *= dragFactor;

      // Update position from velocity
      float moveX = s_DroneVelX * dt;
      float moveY = s_DroneVelY * dt;
      float moveZ = s_DroneVelZ * dt;

      float newX = s_PosX + moveX;
      float newY = s_PosY + moveY;
      float newZ = s_PosZ + moveZ;

      if (g_CamCollision) {
        float radius = 0.5f;
        int iterations = 3;

        while (iterations-- > 0) {
          float moveDist = sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
          if (moveDist < 0.001f)
            break; // Reached destination

          int handle = invoke<int>(0x28579D1B8F8AAC80, s_PosX, s_PosY, s_PosZ,
                                   s_PosX + moveX, s_PosY + moveY,
                                   s_PosZ + moveZ, radius, -1, s_FrozenPed, 7);
          int hit = 0;
          Vector3 hitCoords, surfNormal;
          int entHit = 0;
          invoke<int>(0x3D87450E15D98694, handle, &hit, &hitCoords, &surfNormal,
                      &entHit);

          if (!hit || (surfNormal.x == 0.0f && surfNormal.y == 0.0f &&
                       surfNormal.z == 0.0f)) {
            s_PosX += moveX;
            s_PosY += moveY;
            s_PosZ += moveZ;
            break;
          }

          // Push the safe starting position out of the hit plane strictly
          float distA = (s_PosX - hitCoords.x) * surfNormal.x +
                        (s_PosY - hitCoords.y) * surfNormal.y +
                        (s_PosZ - hitCoords.z) * surfNormal.z;
          if (distA < radius) {
            float pushOutA = radius - distA + 0.0001f;
            s_PosX += surfNormal.x * pushOutA;
            s_PosY += surfNormal.y * pushOutA;
            s_PosZ += surfNormal.z * pushOutA;
          }

          // Push the intended destination out of the hit plane
          float tgtX = s_PosX + moveX;
          float tgtY = s_PosY + moveY;
          float tgtZ = s_PosZ + moveZ;
          float distToPlane = (tgtX - hitCoords.x) * surfNormal.x +
                              (tgtY - hitCoords.y) * surfNormal.y +
                              (tgtZ - hitCoords.z) * surfNormal.z;
          if (distToPlane < radius) {
            float pushOut = radius - distToPlane + 0.0001f;
            tgtX += surfNormal.x * pushOut;
            tgtY += surfNormal.y * pushOut;
            tgtZ += surfNormal.z * pushOut;
          }

          // Re-calculate the remaining movement vector
          moveX = tgtX - s_PosX;
          moveY = tgtY - s_PosY;
          moveZ = tgtZ - s_PosZ;

          // Dampen drone velocity into the wall
          float dotVel = s_DroneVelX * surfNormal.x +
                         s_DroneVelY * surfNormal.y +
                         s_DroneVelZ * surfNormal.z;
          if (dotVel < 0) {
            s_DroneVelX -= dotVel * surfNormal.x;
            s_DroneVelY -= dotVel * surfNormal.y;
            s_DroneVelZ -= dotVel * surfNormal.z;
          }
        }

        newX = s_PosX;
        newY = s_PosY;
        newZ = s_PosZ;
      }
      s_PosX = newX;
      s_PosY = newY;
      s_PosZ = newZ;
    } else {

      float speed = g_CamSpeed * speedMult * dt * 40.0f;
      float moveX =
          (fwdX * moveForward + rightX * moveRight + upX * moveUp) * speed;
      float moveY =
          (fwdY * moveForward + rightY * moveRight + upY * moveUp) * speed;
      float moveZ =
          (fwdZ * moveForward + rightZ * moveRight + upZ * moveUp) * speed;

      float newX = s_PosX + moveX;
      float newY = s_PosY + moveY;
      float newZ = s_PosZ + moveZ;

      if (g_CamCollision) {
        float radius = 0.5f;
        int iterations = 3;

        while (iterations-- > 0) {
          float moveDist = sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
          if (moveDist < 0.001f)
            break; // Reached destination

          int handle = invoke<int>(0x28579D1B8F8AAC80, s_PosX, s_PosY, s_PosZ,
                                   s_PosX + moveX, s_PosY + moveY,
                                   s_PosZ + moveZ, radius, -1, s_FrozenPed, 7);
          int hit = 0;
          Vector3 hitCoords, surfNormal;
          int entHit = 0;
          invoke<int>(0x3D87450E15D98694, handle, &hit, &hitCoords, &surfNormal,
                      &entHit);

          if (!hit || (surfNormal.x == 0.0f && surfNormal.y == 0.0f &&
                       surfNormal.z == 0.0f)) {
            s_PosX += moveX;
            s_PosY += moveY;
            s_PosZ += moveZ;
            break;
          }

          // Push the safe starting position out of the hit plane strictly
          float distA = (s_PosX - hitCoords.x) * surfNormal.x +
                        (s_PosY - hitCoords.y) * surfNormal.y +
                        (s_PosZ - hitCoords.z) * surfNormal.z;
          if (distA < radius) {
            float pushOutA = radius - distA + 0.0001f;
            s_PosX += surfNormal.x * pushOutA;
            s_PosY += surfNormal.y * pushOutA;
            s_PosZ += surfNormal.z * pushOutA;
          }

          // Push the intended destination out of the hit plane
          float tgtX = s_PosX + moveX;
          float tgtY = s_PosY + moveY;
          float tgtZ = s_PosZ + moveZ;
          float distToPlane = (tgtX - hitCoords.x) * surfNormal.x +
                              (tgtY - hitCoords.y) * surfNormal.y +
                              (tgtZ - hitCoords.z) * surfNormal.z;
          if (distToPlane < radius) {
            float pushOut = radius - distToPlane + 0.0001f;
            tgtX += surfNormal.x * pushOut;
            tgtY += surfNormal.y * pushOut;
            tgtZ += surfNormal.z * pushOut;
          }

          // Re-calculate the remaining movement vector
          moveX = tgtX - s_PosX;
          moveY = tgtY - s_PosY;
          moveZ = tgtZ - s_PosZ;
        }

        newX = s_PosX;
        newY = s_PosY;
        newZ = s_PosZ;
      }
      s_PosX = newX;
      s_PosY = newY;
      s_PosZ = newZ;
    }

    if (g_MovePlayerWithCamera && ENTITY::DOES_ENTITY_EXIST(s_FrozenPed)) {
      ENTITY::SET_ENTITY_COORDS_NO_OFFSET(s_FrozenPed, s_PosX, s_PosY, s_PosZ,
                                          FALSE, FALSE, FALSE);
    }

  } // end IGCS session guard

  // Check if the script updated s_PosX/Y/Z manually and sync the attachment
  // offset if so
  if (s_IsAttached && targetHandle != 0) {
    Vector3 attachedPos =
        invoke<Vector3>(0x1899F328B0E12848, targetHandle, s_LocalOffset.x,
                        s_LocalOffset.y, s_LocalOffset.z);
    if (abs(s_PosX - attachedPos.x) > 0.001f ||
        abs(s_PosY - attachedPos.y) > 0.001f ||
        abs(s_PosZ - attachedPos.z) > 0.001f) {
      s_LocalOffset = invoke<Vector3>(0x2274BC1C4885E333, targetHandle, s_PosX,
                                      s_PosY, s_PosZ);
      invoke<Void>(0xFEDB7D269E8C60E3, s_Cam, targetHandle, s_LocalOffset.x,
                   s_LocalOffset.y, s_LocalOffset.z, TRUE);
    }
  }

  // ---- Apply to camera ----

  if (g_RotationEngine && !s_IsAttached && !IGCS_IsSessionActive()) {
    // Quaternion Entity Anchor Architecture
    if (s_RotationAnchor == 0) {
      Hash anchorModel = 0x19B81B74; // "prop_paper_bag"

      // Asynchronously request the model into memory
      if (!STREAMING::HAS_MODEL_LOADED(anchorModel)) {
        STREAMING::REQUEST_MODEL(anchorModel);
      }

      if (STREAMING::HAS_MODEL_LOADED(anchorModel)) {
        s_RotationAnchor = OBJECT::CREATE_OBJECT_NO_OFFSET(
            anchorModel, s_PosX, s_PosY, s_PosZ, FALSE, FALSE, FALSE);
        if (s_RotationAnchor != 0) {
          ENTITY::SET_ENTITY_VISIBLE(s_RotationAnchor, FALSE, FALSE);
          ENTITY::FREEZE_ENTITY_POSITION(s_RotationAnchor, TRUE);
          ENTITY::SET_ENTITY_COLLISION(s_RotationAnchor, FALSE, FALSE);
          CAM::ATTACH_CAM_TO_ENTITY(s_Cam, s_RotationAnchor, 0.0f, 0.0f, 0.0f,
                                    TRUE);
        }
      }
    }

    if (s_RotationAnchor != 0) {
      // Teleport the invisible anchor to the calculated camera flight
      // coordinates (plus IGCS offset if any)
      ENTITY::SET_ENTITY_COORDS_NO_OFFSET(
          s_RotationAnchor, s_PosX + s_IgcsOffsetX, s_PosY + s_IgcsOffsetY,
          s_PosZ + s_IgcsOffsetZ, FALSE, FALSE, FALSE);

      // Natively apply the raw Quaternion Matrix to the Anchor (bypassing
      // SET_CAM_ROT's Euler limits natively)
      invoke<Void>(0x77B21BE7AC540F07, s_RotationAnchor, s_AcroMatrix.x,
                   s_AcroMatrix.y, s_AcroMatrix.z, s_AcroMatrix.w);

      // Strip any residual Euler drift from the camera itself so it identically
      // inherits the Anchor's exact rotation
      CAM::SET_CAM_ROT(s_Cam, 0.0f + s_IgcsPitchOffset, 0.0f + s_IgcsRollOffset,
                       0.0f + s_IgcsYawOffset, 2);
    } else {
      // Model is still streaming in, temporarily fallback to Basic Eulerian
      // rendering to prevent rendering/movement freezing
      CAM::SET_CAM_COORD(s_Cam, s_PosX + s_IgcsOffsetX, s_PosY + s_IgcsOffsetY,
                         s_PosZ + s_IgcsOffsetZ);
      CAM::SET_CAM_ROT(s_Cam, s_Pitch + s_IgcsPitchOffset,
                       g_CamRoll + s_IgcsRollOffset, s_Yaw + s_IgcsYawOffset,
                       2);
    }
  } else {
    // Standard Eulerian Flight (or Rigid Mode)
    if (s_RotationAnchor != 0) {
      if (!s_IsAttached || IGCS_IsSessionActive()) {
        // Only detach if Rigid Mode didn't recently hijack the attachment
        // Or temporarily detach if IGCS is running to bypass forced local
        // offsets
        CAM::DETACH_CAM(s_Cam);
      }
      if (ENTITY::DOES_ENTITY_EXIST(s_RotationAnchor)) {
        ENTITY::DELETE_ENTITY(&s_RotationAnchor);
      }
      s_RotationAnchor = 0;
    }

    static bool s_IgcsTemporarilyOffset = false;

    if (IGCS_IsSessionActive()) {
      if (s_IsAttached) {
        // Translate the camera's intended world position into a temporary local offset for the entity
        Vector3 tempLocalOffset = invoke<Vector3>(0x2274BC1C4885E333, targetHandle, s_PosX + s_IgcsOffsetX,
                                        s_PosY + s_IgcsOffsetY, s_PosZ + s_IgcsOffsetZ);
        // Force the engine to hold the camera natively on the vehicle at this specific DoF pixel offset
        invoke<Void>(0xFEDB7D269E8C60E3, s_Cam, targetHandle, tempLocalOffset.x,
                     tempLocalOffset.y, tempLocalOffset.z, TRUE);
        s_IgcsTemporarilyOffset = true;
      } else {
        CAM::SET_CAM_COORD(s_Cam, s_PosX + s_IgcsOffsetX, s_PosY + s_IgcsOffsetY,
                           s_PosZ + s_IgcsOffsetZ);
      }
    } else {
      if (s_IsAttached && s_IgcsTemporarilyOffset) {
        // Session ended! Revert the ATTACH offset back to the clean s_LocalOffset tracking lock.
        invoke<Void>(0xFEDB7D269E8C60E3, s_Cam, targetHandle, s_LocalOffset.x,
                     s_LocalOffset.y, s_LocalOffset.z, TRUE); // ATTACH_CAM_TO_ENTITY
        s_IgcsTemporarilyOffset = false;
      } else if (!s_IsAttached) {
        CAM::SET_CAM_COORD(s_Cam, s_PosX, s_PosY, s_PosZ);
      }
    }

    // Apply Standard Native Euler tracking (Allows Gimbal clipping limits
    // natively)
    CAM::SET_CAM_ROT(s_Cam, s_Pitch + s_IgcsPitchOffset,
                     g_CamRoll + s_IgcsRollOffset, s_Yaw + s_IgcsYawOffset, 2);
  }

  // Set FOV (real-time) via hash because SDK version is mismatched
  invoke<Void>(0xB13C14F66A00D047, s_Cam, g_CamFOV);

  // ---- Depth of Field ----

  if (g_DoFEnabled) {
    CAM::SET_USE_HI_DOF();

    if (g_DoFAutofocus) {
      // Calculate forward vector from yaw/pitch
      float yawRad = s_Yaw * DEG2RAD;
      float pitchRad = s_Pitch * DEG2RAD;

      float dirX = -sin(yawRad) * cos(pitchRad);
      float dirY = cos(yawRad) * cos(pitchRad);
      float dirZ = sin(pitchRad);

      // Raycast end coordinates
      float endX = s_PosX + dirX * 1000.0f;
      float endY = s_PosY + dirY * 1000.0f;
      float endZ = s_PosZ + dirZ * 1000.0f;

      // START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE (flags: -1 = intersect
      // everything, p8 = 7)
      int handle = invoke<int>(0x377906D8A31E5586, s_PosX, s_PosY, s_PosZ, endX,
                               endY, endZ, -1, s_FrozenPed, 7);

      int hit = 0;
      Vector3 endCoords, surfaceNormal;
      int entityHit = 0;

      // GET_SHAPE_TEST_RESULT
      int result = invoke<int>(0x3D87450E15D98694, handle, &hit, &endCoords,
                               &surfaceNormal, &entityHit);

      if (result == 2 && hit != 0) {
        // Calculate distance
        float dx = endCoords.x - s_PosX;
        float dy = endCoords.y - s_PosY;
        float dz = endCoords.z - s_PosZ;
        g_DoFFocusDist = sqrt(dx * dx + dy * dy + dz * dz);
      } else {
        g_DoFFocusDist = 1000.0f; // Default if nothing hit
      }
    }

    // Use standard Depth of Field planes since Shallow DoF hashes are missing
    float nearFocus = g_DoFFocusDist;
    float nearBlur = nearFocus - g_DoFMaxNearInFocus;
    float farFocus = nearFocus + g_DoFMaxFarInFocus;
    float farBlur = farFocus + 15.0f;

    if (nearBlur < 0.0f)
      nearBlur = 0.0f;

    CAM::SET_CAM_NEAR_DOF(s_Cam, nearBlur);
    CAM::SET_CAM_FAR_DOF(s_Cam, farBlur);

    CAM::SET_CAM_DOF_PLANES(s_Cam, nearBlur, nearFocus, farFocus, farBlur);
  } else if (g_FreeCamActive || IGCS_IsConnected()) {
    // Force disable game's automatic DoF by explicitly taking control and
    // setting 0 blur
    CAM::SET_USE_HI_DOF();

    // Near planes: 0, Far planes: 10000
    CAM::SET_CAM_NEAR_DOF(s_Cam, 0.0f);
    CAM::SET_CAM_FAR_DOF(s_Cam, 10000.0f);
    CAM::SET_CAM_DOF_PLANES(s_Cam, 0.0f, 0.0f, 10000.0f, 10000.0f);

    // SET_CAM_DOF_STRENGTH to 0
    invoke<Void>(0x5EE29B4D7D5DF897, s_Cam, 0.0f);
  } else {
    // Freecam is OFF and IGCS is disconnected.
    // Explicitly release script control over DoF so the game's default dynamic
    // DoF works again. SET_CAM_USE_SHALLOW_DOF_MODE
    invoke<Void>(0x16A96863A17552BB, s_Cam, FALSE);
  }
}

// ============================================================
//  Time & Weather Update
// ============================================================

void UpdateTimeWeather() {
  if (g_TimePaused) {
    TIME::PAUSE_CLOCK(TRUE);
    TIME::SET_CLOCK_TIME(g_TimeHour, g_TimeMinute, 0);
  } else {
    TIME::PAUSE_CLOCK(FALSE);
  }

  // Timelapse logic
  if (g_TimelapseMode > 0) {
    static DWORD lastLapseTick = GetTickCount();
    DWORD currentTick = GetTickCount();

    DWORD interval = 100; // 0 = Off. 1 = Slow.
    if (g_TimelapseMode == 2)
      interval = 25; // Medium
    if (g_TimelapseMode == 3)
      interval = 5; // Fast

    if (currentTick - lastLapseTick >= interval) {
      g_TimeMinute++;
      if (g_TimeMinute > 59) {
        g_TimeMinute = 0;
        g_TimeHour = (g_TimeHour + 1) % 24;
      }
      TIME::SET_CLOCK_TIME(g_TimeHour, g_TimeMinute, 0);
      lastLapseTick = currentTick;
    }
  }

  // Weather Blending logic
  if (g_BlendWeatherActive && g_WeatherBlend > 0.0f) {
    Hash w1 = GAMEPLAY::GET_HASH_KEY((char *)g_WeatherNames[g_Weather1Index]);
    Hash w2 = GAMEPLAY::GET_HASH_KEY((char *)g_WeatherNames[g_Weather2Index]);
    invoke<Void>(0x578C752848ECFA0C, w1, w2,
                 g_WeatherBlend); // _SET_WEATHER_TYPE_TRANSITION
  }
}

// ============================================================
//  Global Effects (run every frame regardless of freecam)
// ============================================================

void UpdateGlobalEffects() {
  // Freeze World
  if (g_FreezeWorld) {
    GAMEPLAY::SET_TIME_SCALE(0.0f);
  }

  // Info Overlay
  if (g_ShowInfoOverlay) {
    DrawInfoOverlay();
  }

  // Hide HUD
  if (g_HideHUD) {
    UI::HIDE_HUD_AND_RADAR_THIS_FRAME();
  }

  // Hide Player
  static bool s_PlayerWasHiddenByUs = false;
  Ped playerPed = PLAYER::PLAYER_PED_ID();
  if (ENTITY::DOES_ENTITY_EXIST(playerPed)) {
    if (g_HidePlayer) {
      ENTITY::SET_ENTITY_VISIBLE(playerPed, FALSE, FALSE);
      s_PlayerWasHiddenByUs = true;
    } else if (s_PlayerWasHiddenByUs) {
      ENTITY::SET_ENTITY_VISIBLE(playerPed, TRUE, FALSE);
      s_PlayerWasHiddenByUs = false;
    }
  }
}

// ============================================================
//  IGCS Connector Helpers
// ============================================================

void IGCS_ResetOffsets() {
  s_IgcsOffsetX = 0.0f;
  s_IgcsOffsetY = 0.0f;
  s_IgcsOffsetZ = 0.0f;
  s_IgcsPitchOffset = 0.0f;
  s_IgcsYawOffset = 0.0f;
  s_IgcsRollOffset = 0.0f;
}

void IGCS_ShiftCamera(float rightStep, float upStep, float forwardStep,
                      float pitchDelta, float yawDelta, float rollDelta) {
  if (!g_FreeCamActive)
    return;

  // Use core rotation + IGCS offset rotation
  float curPitch = s_Pitch + s_IgcsPitchOffset;
  float curYaw = s_Yaw + s_IgcsYawOffset;
  float curRoll = g_CamRoll + s_IgcsRollOffset;

  // Compute direction vectors from current yaw/pitch/roll
  float yawRad = curYaw * (3.14159265f / 180.0f);
  float pitchRad = curPitch * (3.14159265f / 180.0f);
  float rollRad = curRoll * (3.14159265f / 180.0f);

  float cp = cosf(pitchRad), sp = sinf(pitchRad);
  float cy = cosf(yawRad), sy = sinf(yawRad);
  float cr = cosf(rollRad), sr = sinf(rollRad);

  // Forward
  float fwdX = -sy * cp;
  float fwdY = cy * cp;
  float fwdZ = sp;

  // Right (accounts for roll)
  float rightX = cy * cr + sy * sp * sr;
  float rightY = sy * cr - cy * sp * sr;
  float rightZ = cp * sr;

  // Up (cross product: forward x right, accounts for pitch and roll)
  float camUpX = -cy * sr + sy * sp * cr;
  float camUpY = -sy * sr - cy * sp * cr;
  float camUpZ = cp * cr;

  if (IGCS_IsSessionActive()) {
    s_IgcsOffsetX += rightX * rightStep + camUpX * upStep + fwdX * forwardStep;
    s_IgcsOffsetY += rightY * rightStep + camUpY * upStep + fwdY * forwardStep;
    s_IgcsOffsetZ += rightZ * rightStep + camUpZ * upStep + fwdZ * forwardStep;
    s_IgcsPitchOffset += pitchDelta;
    s_IgcsYawOffset += yawDelta;
    s_IgcsRollOffset += rollDelta;
  } else {
    s_PosX += rightX * rightStep + camUpX * upStep + fwdX * forwardStep;
    s_PosY += rightY * rightStep + camUpY * upStep + fwdY * forwardStep;
    s_PosZ += rightZ * rightStep + camUpZ * upStep + fwdZ * forwardStep;
    s_Pitch += pitchDelta;
    s_Yaw += yawDelta;
    g_CamRoll += rollDelta;
  }
}

void IGCS_SetCameraPosition(float posX, float posY, float posZ, float pitch,
                            float yaw, float roll) {
  if (IGCS_IsSessionActive()) {
    s_IgcsOffsetX = posX - s_PosX;
    s_IgcsOffsetY = posY - s_PosY;
    s_IgcsOffsetZ = posZ - s_PosZ;
    s_IgcsPitchOffset = pitch - s_Pitch;
    s_IgcsYawOffset = yaw - s_Yaw;
    s_IgcsRollOffset = roll - g_CamRoll;
  } else {
    s_PosX = posX;
    s_PosY = posY;
    s_PosZ = posZ;
    s_Pitch = pitch;
    s_Yaw = yaw;
    g_CamRoll = roll;
  }
}

void GetCameraState(float &posX, float &posY, float &posZ, float &pitch,
                    float &yaw, float &roll) {
  posX = s_PosX;
  posY = s_PosY;
  posZ = s_PosZ;
  pitch = s_Pitch;
  yaw = s_Yaw;
  roll = g_CamRoll;
}
