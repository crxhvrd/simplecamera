/*
        GTA V Free Camera / Photo Mode Plugin
        In-Game Menu System Implementation

        NativeTrainer-style hierarchical menu with full keyboard + controller
   support. Five categories: Activation, Movement, Lens, Depth of Field, Time &
   Weather.
*/

#include "menu.h"
#include "camera.h"
#include "fx_capture.h"
#include "keyboard.h"
#include "sequence.h"

#include <cstdio>
#include <string>

#pragma warning(disable : 4244 4305)

// ============================================================
//  Status Text
// ============================================================

static std::string s_StatusText;
static DWORD s_StatusTextMaxTicks = 0;

// Set while any configuration menu is open (see camera.h). UpdateFreeCamera
// reads this to suppress menu-shared controller input (D-Pad zoom).
bool g_MenuOpen = false;

void SetStatusText(std::string str, DWORD time) {
  s_StatusText = str;
  s_StatusTextMaxTicks = GetTickCount() + time;
}

void UpdateStatusText() {
  if (GetTickCount() < s_StatusTextMaxTicks) {
    UI::SET_TEXT_FONT(0);
    UI::SET_TEXT_SCALE(0.55, 0.55);
    UI::SET_TEXT_COLOUR(255, 255, 255, 255);
    UI::SET_TEXT_WRAP(0.0, 1.0);
    UI::SET_TEXT_CENTRE(1);
    UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
    UI::SET_TEXT_EDGE(1, 0, 0, 0, 205);
    UI::_SET_TEXT_ENTRY((char *)"STRING");
    UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)s_StatusText.c_str());
    UI::_DRAW_TEXT(0.5, 0.5);
  }
}

// ============================================================
//  Settings
// ============================================================

static int g_MenuKey = VK_F5; // default

// Resolve the full path to SimpleCamera.ini sitting next to the ASI. Single
// source of truth for both load and save so they can never disagree.
static void GetIniPath(char *path, size_t cap) {
  HMODULE hMod = NULL;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCSTR)GetIniPath, &hMod);
  GetModuleFileNameA(hMod, path, (DWORD)cap);
  char *last = strrchr(path, '\\');
  if (last)
    strcpy_s(last + 1, cap - (last + 1 - path), "SimpleCamera.ini");
}

// GetPrivateProfileInt stores ints only; floats need string round-tripping.
static float IniReadFloat(const char *section, const char *key, float def,
                          const char *path) {
  char buf[64], defbuf[64];
  sprintf_s(defbuf, "%g", def);
  GetPrivateProfileStringA(section, key, defbuf, buf, sizeof(buf), path);
  return (float)atof(buf);
}

static bool IniReadBool(const char *section, const char *key, bool def,
                        const char *path) {
  return GetPrivateProfileIntA(section, key, def ? 1 : 0, path) != 0;
}

void LoadSettings() {
  char path[MAX_PATH];
  GetIniPath(path, MAX_PATH);

  // ---- Controls ----
  g_MenuKey = GetPrivateProfileIntA("Controls", "MenuKey", VK_F5, path);
  // Camera Sequence hotkeys — inert outside Sequence mode.
  g_SeqHotkeyAdd = GetPrivateProfileIntA("Controls", "SequenceAddKey", VK_F6, path);
  g_SeqHotkeyPlay = GetPrivateProfileIntA("Controls", "SequencePlayKey", VK_F7, path);
  g_SeqHotkeyStop = GetPrivateProfileIntA("Controls", "SequenceStopKey", VK_F8, path);
  g_SeqHotkeyNext = GetPrivateProfileIntA("Controls", "SequenceNextKey", VK_F9, path);

  // ---- Camera ---- (defaults match the global initializers in camera.cpp)
  g_CamSpeed = IniReadFloat("Camera", "CamSpeed", g_CamSpeed, path);
  g_CamSensitivity = IniReadFloat("Camera", "CamSensitivity", g_CamSensitivity, path);
  g_CamFOV = IniReadFloat("Camera", "CamFOV", g_CamFOV, path);
  g_ZoomSpeed = IniReadFloat("Camera", "ZoomSpeed", g_ZoomSpeed, path);
  g_RollSpeed = IniReadFloat("Camera", "RollSpeed", g_RollSpeed, path);
  g_CamCollision = IniReadBool("Camera", "Collision", g_CamCollision, path);
  g_LockHeight = IniReadBool("Camera", "LockHeight", g_LockHeight, path);
  g_RotationEngine = IniReadBool("Camera", "AcrobaticRotation", g_RotationEngine, path);
  g_WalkMode = IniReadBool("Camera", "WalkMode", g_WalkMode, path);
  g_WalkHeight = IniReadFloat("Camera", "WalkHeight", g_WalkHeight, path);
  g_FollowRigidMode = IniReadBool("Camera", "FollowRigidMode", g_FollowRigidMode, path);

  // ---- Auto Drive ---- (mode/speed/style persist; the enabled flag is
  // transient — it's never auto-engaged on load)
  g_AutoDriveMode = GetPrivateProfileIntA("AutoDrive", "Mode", g_AutoDriveMode, path);
  g_AutoDriveSpeed = IniReadFloat("AutoDrive", "Speed", g_AutoDriveSpeed, path);
  g_AutoDriveStyleIndex = GetPrivateProfileIntA("AutoDrive", "StyleIndex", g_AutoDriveStyleIndex, path);
  if (g_AutoDriveStyleIndex < 0 || g_AutoDriveStyleIndex >= g_AutoDriveStyleCount)
    g_AutoDriveStyleIndex = 0;

  // ---- Drone ----
  g_DroneMode = IniReadBool("Drone", "Enabled", g_DroneMode, path);
  g_DroneDrag = IniReadFloat("Drone", "Drag", g_DroneDrag, path);
  g_DroneAcceleration = IniReadFloat("Drone", "Acceleration", g_DroneAcceleration, path);
  g_DroneGravity = IniReadFloat("Drone", "Gravity", g_DroneGravity, path);
  g_DroneBanking = IniReadFloat("Drone", "Banking", g_DroneBanking, path);
  g_DroneRotSmoothing = IniReadFloat("Drone", "RotSmoothing", g_DroneRotSmoothing, path);
  g_DroneFovSmoothing = IniReadFloat("Drone", "FovSmoothing", g_DroneFovSmoothing, path);

  // ---- Shake ----
  g_ShakeEnabled = IniReadBool("Shake", "Enabled", g_ShakeEnabled, path);
  g_ShakePreset = GetPrivateProfileIntA("Shake", "Preset", g_ShakePreset, path);
  g_ShakeAmp = IniReadFloat("Shake", "Amplitude", g_ShakeAmp, path);
  g_ShakeFreq = IniReadFloat("Shake", "Frequency", g_ShakeFreq, path);
  g_ShakeSpeedAmpCoupling = IniReadFloat("Shake", "SpeedAmpCoupling", g_ShakeSpeedAmpCoupling, path);
  g_ShakeSpeedFreqCoupling = IniReadFloat("Shake", "SpeedFreqCoupling", g_ShakeSpeedFreqCoupling, path);
  g_ShakeRotWeight = IniReadFloat("Shake", "RotWeight", g_ShakeRotWeight, path);
  g_ShakePosWeight = IniReadFloat("Shake", "PosWeight", g_ShakePosWeight, path);
  g_ShakeStopWhenStill = IniReadBool("Shake", "StopWhenStill", g_ShakeStopWhenStill, path);

  // ---- Depth of Field ----
  g_DoFEnabled = IniReadBool("DoF", "Enabled", g_DoFEnabled, path);
  g_DoFAutofocus = IniReadBool("DoF", "Autofocus", g_DoFAutofocus, path);
  g_DoFFocusDist = IniReadFloat("DoF", "FocusDist", g_DoFFocusDist, path);
  g_DoFMaxNearInFocus = IniReadFloat("DoF", "NearRange", g_DoFMaxNearInFocus, path);
  g_DoFMaxFarInFocus = IniReadFloat("DoF", "FarRange", g_DoFMaxFarInFocus, path);

  // ---- Misc ----
  g_HideHUD = IniReadBool("Misc", "HideHUD", g_HideHUD, path);
  g_HidePlayer = IniReadBool("Misc", "HidePlayer", g_HidePlayer, path);
  g_MovePlayerWithCamera = IniReadBool("Misc", "MovePlayerWithCamera", g_MovePlayerWithCamera, path);
  g_RememberCamPosition = IniReadBool("Misc", "RememberCamPosition", g_RememberCamPosition, path);
  g_ShowInfoOverlay = IniReadBool("Misc", "ShowInfoOverlay", g_ShowInfoOverlay, path);
  g_DisableVehicleShake = IniReadBool("Misc", "DisableVehicleShake", g_DisableVehicleShake, path);
  g_ShowLockedEntityMarker = IniReadBool("Misc", "ShowLockedEntityMarker", g_ShowLockedEntityMarker, path);
}

// Persist all tunable settings to the INI. Called from the Misc menu's
// "Save Settings to INI" row. Transient state (active mode, follow target,
// live time/weather, freeze) is intentionally NOT saved.
void SaveSettings() {
  char path[MAX_PATH];
  GetIniPath(path, MAX_PATH);
  char buf[64];
  auto wf = [&](const char *sec, const char *key, float v) {
    sprintf_s(buf, "%g", v);
    WritePrivateProfileStringA(sec, key, buf, path);
  };
  auto wb = [&](const char *sec, const char *key, bool v) {
    WritePrivateProfileStringA(sec, key, v ? "1" : "0", path);
  };
  auto wi = [&](const char *sec, const char *key, int v) {
    sprintf_s(buf, "%d", v);
    WritePrivateProfileStringA(sec, key, buf, path);
  };

  wf("Camera", "CamSpeed", g_CamSpeed);
  wf("Camera", "CamSensitivity", g_CamSensitivity);
  wf("Camera", "CamFOV", g_CamFOV);
  wf("Camera", "ZoomSpeed", g_ZoomSpeed);
  wf("Camera", "RollSpeed", g_RollSpeed);
  wb("Camera", "Collision", g_CamCollision);
  wb("Camera", "LockHeight", g_LockHeight);
  wb("Camera", "AcrobaticRotation", g_RotationEngine);
  wb("Camera", "WalkMode", g_WalkMode);
  wf("Camera", "WalkHeight", g_WalkHeight);
  wb("Camera", "FollowRigidMode", g_FollowRigidMode);

  wi("AutoDrive", "Mode", g_AutoDriveMode);
  wf("AutoDrive", "Speed", g_AutoDriveSpeed);
  wi("AutoDrive", "StyleIndex", g_AutoDriveStyleIndex);

  wb("Drone", "Enabled", g_DroneMode);
  wf("Drone", "Drag", g_DroneDrag);
  wf("Drone", "Acceleration", g_DroneAcceleration);
  wf("Drone", "Gravity", g_DroneGravity);
  wf("Drone", "Banking", g_DroneBanking);
  wf("Drone", "RotSmoothing", g_DroneRotSmoothing);
  wf("Drone", "FovSmoothing", g_DroneFovSmoothing);

  wb("Shake", "Enabled", g_ShakeEnabled);
  wi("Shake", "Preset", g_ShakePreset);
  wf("Shake", "Amplitude", g_ShakeAmp);
  wf("Shake", "Frequency", g_ShakeFreq);
  wf("Shake", "SpeedAmpCoupling", g_ShakeSpeedAmpCoupling);
  wf("Shake", "SpeedFreqCoupling", g_ShakeSpeedFreqCoupling);
  wf("Shake", "RotWeight", g_ShakeRotWeight);
  wf("Shake", "PosWeight", g_ShakePosWeight);
  wb("Shake", "StopWhenStill", g_ShakeStopWhenStill);

  wb("DoF", "Enabled", g_DoFEnabled);
  wb("DoF", "Autofocus", g_DoFAutofocus);
  wf("DoF", "FocusDist", g_DoFFocusDist);
  wf("DoF", "NearRange", g_DoFMaxNearInFocus);
  wf("DoF", "FarRange", g_DoFMaxFarInFocus);

  wb("Misc", "HideHUD", g_HideHUD);
  wb("Misc", "HidePlayer", g_HidePlayer);
  wb("Misc", "MovePlayerWithCamera", g_MovePlayerWithCamera);
  wb("Misc", "RememberCamPosition", g_RememberCamPosition);
  wb("Misc", "ShowInfoOverlay", g_ShowInfoOverlay);
  wb("Misc", "DisableVehicleShake", g_DisableVehicleShake);
  wb("Misc", "ShowLockedEntityMarker", g_ShowLockedEntityMarker);
}

// Restore every persisted tunable to its factory default — the same values as
// the global initializers in camera.cpp. Does NOT write the INI (the user can
// Save afterward if they want the reset persisted) and leaves transient state
// (active mode, follow target, live time/weather) untouched.
static void ResetSettingsToDefaults() {
  g_CamSpeed = 1.0f;
  g_CamSensitivity = 1.0f;
  g_CamFOV = 50.0f;
  g_CamRoll = 0.0f;
  g_RollSpeed = 1.0f;
  g_ZoomSpeed = 1.0f;
  g_CamCollision = false;
  g_LockHeight = false;
  g_RotationEngine = false;
  g_WalkMode = false;
  g_WalkHeight = 1.7f;
  g_FollowRigidMode = false;

  g_AutoDriveMode = 0;
  g_AutoDriveSpeed = 20.0f;
  g_AutoDriveStyleIndex = 0;

  g_DroneMode = false;
  g_DroneDrag = 3.0f;
  g_DroneAcceleration = 15.0f;
  g_DroneGravity = 0.0f;
  g_DroneBanking = 15.0f;
  g_DroneRotSmoothing = 8.0f;
  g_DroneFovSmoothing = 5.0f;

  g_ShakeEnabled = false;
  g_ShakePreset = 0;
  g_ShakeAmp = 0.0f;
  g_ShakeFreq = 4.0f;
  g_ShakeSpeedAmpCoupling = 1.0f;
  g_ShakeSpeedFreqCoupling = 0.5f;
  g_ShakeRotWeight = 1.0f;
  g_ShakePosWeight = 1.0f;
  g_ShakeStopWhenStill = false;

  g_DoFEnabled = false;
  g_DoFAutofocus = false;
  g_DoFFocusDist = 10.0f;
  g_DoFMaxNearInFocus = 0.5f;
  g_DoFMaxFarInFocus = 5.0f;

  g_MovePlayerWithCamera = false;
  g_RememberCamPosition = false;
  g_ShowInfoOverlay = false;
  g_DisableVehicleShake = false;
  g_ShowLockedEntityMarker = true;
}

// ============================================================
//  Menu Toggle
// ============================================================

bool IsMenuTogglePressed() { return IsKeyJustUp(g_MenuKey); }

// Controller has no F5, so LB + RB opens the menu. Edge-triggered (one bumper
// held, the other just pressed) so holding both — or rolling with a single
// bumper — doesn't retrigger every frame. 205 = INPUT_FRONTEND_LB, 206 = RB.
bool IsControllerMenuCombo() {
  bool lb = PAD::IS_DISABLED_CONTROL_PRESSED(0, 205);
  bool rb = PAD::IS_DISABLED_CONTROL_PRESSED(0, 206);
  bool lbJust = PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, 205);
  bool rbJust = PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, 206);
  return (lb && rbJust) || (rb && lbJust);
}

// LB + B leaves Free Camera back to the mode picker without opening the menu.
// Edge-triggered on B (202 = INPUT_FRONTEND_CANCEL) while LB is held.
bool IsControllerExitCombo() {
  return PAD::IS_DISABLED_CONTROL_PRESSED(0, 205) &&
         PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, 202);
}

// ============================================================
//  Drawing Helpers
// ============================================================

static void draw_rect(float x, float y, float w, float h, int r, int g, int b,
                      int a) {
  GRAPHICS::DRAW_RECT((x + (w * 0.5f)), (y + (h * 0.5f)), w, h, r, g, b, a);
}

// Lowest pixel Y reached by any row drawn this frame. DrawMenuLine updates it;
// DrawMenuFooter reads it to park the controls hint right under the last row,
// then resets it. Lets the footer auto-position under any menu without each
// menu having to report its row count.
static float s_MenuMaxBottom = 0.0f;

void DrawMenuLine(std::string caption, std::string value, float lineWidth,
                  float lineHeight, float lineTop, float lineLeft,
                  float textLeft, bool active, bool title, bool rescaleText) {
  // Premium NativeUI Style
  int text_col[4] = {255, 255, 255, 255}; // Pure white for inactive text
  int rect_col[4] = {0, 0, 0, 180};       // Translucent sleek black background
  float text_scale = 0.35f;
  int font = 4; // Font 4: Chalet Comprime Cologne (Standard GTA Menu Font)

  if (active) {
    // Pure white row with black text
    text_col[0] = 0;
    text_col[1] = 0;
    text_col[2] = 0;
    rect_col[0] = 255;
    rect_col[1] = 255;
    rect_col[2] = 255;
    rect_col[3] = 255;
  }

  if (title) {
    // Cinematic Title banner
    rect_col[0] = 5;
    rect_col[1] = 5;
    rect_col[2] = 5;
    rect_col[3] = 255;
    text_col[0] = 255;
    text_col[1] = 255;
    text_col[2] = 255;
    text_scale = 0.75f;
    font = 1; // Font 1: House Script / Cursive title
  }

  int screen_w, screen_h;
  GRAPHICS::GET_SCREEN_RESOLUTION(&screen_w, &screen_h);

  // Widen the menu slightly
  lineWidth = 320.0f; // Adjusted to standard NativeUI width

  // Clean up NativeTrainer legacy layout by inferring the actual grid index
  // Legacy code passed active row at 56.0 Y and inactive at 60.0 Y to make them
  // "bulge". The title also overlapped at 18.0. We override it completely here.
  float correctLineTop;
  if (title) {
    correctLineTop = 10.0f; // Pushed up slightly
  } else {
    // Find the clean mathematical row index (0, 1, 2...) safely without cmath
    int index = active ? (int)(((lineTop - 56.0f) / 36.0f) + 0.5f)
                       : (int)(((lineTop - 60.0f) / 36.0f) + 0.5f);
    correctLineTop =
        60.0f +
        (index * 36.0f); // Enforce exactly 36.0f increments starting from 60
  }

  // Enforce zero padding jitter
  float textLeftScaled = (lineLeft + 10.0f) / (float)screen_w;
  float lineWidthScaled = lineWidth / (float)screen_w;
  float lineLeftScaled = lineLeft / (float)screen_w;

  // Exact Native UI Height Constants
  float rowHeight = 36.0f;
  float titleHeight =
      50.0f; // 10.0 + 50.0 = 60.0 (Seamless connection to row 0)

  // Calculate scaled boundaries
  float rectHeightScaled =
      title ? (titleHeight / (float)screen_h) : (rowHeight / (float)screen_h);
  float rectTopScaled = correctLineTop / (float)screen_h;

  // Track the lowest row bottom (in px) so the footer can sit just beneath it.
  float rowBottomPx = correctLineTop + (title ? titleHeight : rowHeight);
  if (rowBottomPx > s_MenuMaxBottom) s_MenuMaxBottom = rowBottomPx;

  // Text Y centering offset (magic numbers relative to font cap height)
  float textYOffsetScaled =
      title ? (12.0f / (float)screen_h) : (6.0f / (float)screen_h);
  float finalTextY = rectTopScaled + textYOffsetScaled;

  // Draw the main rect background precisely without any pixel overlapping
  draw_rect(lineLeftScaled, rectTopScaled, lineWidthScaled, rectHeightScaled,
            rect_col[0], rect_col[1], rect_col[2], rect_col[3]);

  // Draw the caption (left-aligned)
  UI::SET_TEXT_FONT(font);
  UI::SET_TEXT_SCALE(0.0, text_scale);
  UI::SET_TEXT_COLOUR(text_col[0], text_col[1], text_col[2], text_col[3]);
  UI::SET_TEXT_CENTRE(0);
  UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
  UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
  UI::_SET_TEXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)caption.c_str());
  UI::_DRAW_TEXT(textLeftScaled, finalTextY);

  // Draw the value (right-aligned) if present
  if (value.length() > 0) {
    float textRightScaled = (lineLeft + lineWidth - 10.0f) / (float)screen_w;
    UI::SET_TEXT_FONT(font);
    UI::SET_TEXT_SCALE(0.0, text_scale);
    UI::SET_TEXT_COLOUR(text_col[0], text_col[1], text_col[2], text_col[3]);
    UI::SET_TEXT_RIGHT_JUSTIFY(1);
    UI::SET_TEXT_WRAP(0.0f, textRightScaled);
    UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
    UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
    UI::_SET_TEXT_ENTRY((char *)"STRING");
    UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)value.c_str());
    UI::_DRAW_TEXT(0.0f, finalTextY);
  }

  // Draw a subtle 1px border/separator line at the bottom of the row if not
  // active or title
  if (!active && !title) {
    draw_rect(lineLeftScaled,
              rectTopScaled + rectHeightScaled - (1.0f / (float)screen_h),
              lineWidthScaled, (1.0f / (float)screen_h), 255, 255, 255, 12);
  }
}

void DrawMenuValue(std::string caption, std::string value, float lineWidth,
                   float lineHeight, float lineTop, float lineLeft,
                   float textLeft, bool active) {
  // Delegate to DrawMenuLine but pass the value directly instead of padding
  DrawMenuLine(caption, value, lineWidth, lineHeight, lineTop, lineLeft,
               textLeft, active, false);
}

// Draw the controls hint bar just under the last row of whatever menu is open.
// The label set switches between keyboard and controller based on the last
// input device the game saw (IS_INPUT_DISABLED(2) is TRUE on a controller).
// ASCII only — the GTA Chalet Comprime menu font has no arrow/glyph coverage,
// so we spell the directions out. Call once per draw frame, after all rows.
static void DrawMenuFooter() {
  if (s_MenuMaxBottom <= 0.0f) return; // nothing drawn this frame yet
  float top = s_MenuMaxBottom + 5.0f;
  s_MenuMaxBottom = 0.0f; // consume; rows repopulate it next frame

  // DrawMenuLine forces every row to lineLeft=0 / width=320; match that so the
  // footer lines up flush under the menu regardless of the caller's lineWidth.
  const float lineLeft = 0.0f;
  const float lineWidth = 320.0f;

  // IS_INPUT_DISABLED(2) reports TRUE for keyboard/mouse, FALSE for a
  // controller — so a controller is "input NOT disabled" here.
  bool usingPad = invoke<BOOL>(0xA571D46727E2B718, 2) == 0;
  const char *hint =
      usingPad ? "D-Pad: Move / Adjust    A: Select    B: Back"
               : "Arrows: Move / Adjust    Enter: Select    Bksp: Back";

  int screen_w, screen_h;
  GRAPHICS::GET_SCREEN_RESOLUTION(&screen_w, &screen_h);

  float barHeight = 26.0f;
  float topScaled = top / (float)screen_h;
  float heightScaled = barHeight / (float)screen_h;
  float leftScaled = lineLeft / (float)screen_w;
  float widthScaled = lineWidth / (float)screen_w;

  // Dim bar a touch darker than the rows so it reads as chrome, not an option.
  draw_rect(leftScaled, topScaled, widthScaled, heightScaled, 0, 0, 0, 220);

  // Centered, small, light-grey hint text.
  float centerX = (lineLeft + lineWidth * 0.5f) / (float)screen_w;
  float textY = (top + 5.0f) / (float)screen_h;
  UI::SET_TEXT_FONT(0);
  UI::SET_TEXT_SCALE(0.0f, 0.26f);
  UI::SET_TEXT_COLOUR(190, 190, 190, 255);
  UI::SET_TEXT_CENTRE(1);
  UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
  UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
  UI::_SET_TEXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)hint);
  UI::_DRAW_TEXT(centerX, textY);
}

void MenuBeep() {
  AUDIO::PLAY_SOUND_FRONTEND(-1, (char *)"NAV_UP_DOWN",
                             (char *)"HUD_FRONTEND_DEFAULT_SOUNDSET", 0);
}

// ============================================================
//  Button State (Keyboard + Controller)
// ============================================================

static void GetMenuButtons(bool *select, bool *back, bool *up, bool *down,
                           bool *left, bool *right) {
  if (select)
    *select = IsKeyDown(VK_NUMPAD5) || IsKeyDown(VK_RETURN) ||
              PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, 201);
  if (back)
    *back = IsKeyDown(VK_NUMPAD0) || IsKeyDown(VK_BACK) ||
            IsMenuTogglePressed() ||
            PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, 202);
  if (up)
    *up = IsKeyDown(VK_NUMPAD8) || IsKeyDown(VK_UP) ||
          PAD::IS_DISABLED_CONTROL_PRESSED(0, 188);
  if (down)
    *down = IsKeyDown(VK_NUMPAD2) || IsKeyDown(VK_DOWN) ||
            PAD::IS_DISABLED_CONTROL_PRESSED(0, 187);
  if (right)
    *right = IsKeyDown(VK_NUMPAD6) || IsKeyDown(VK_RIGHT) ||
             PAD::IS_DISABLED_CONTROL_PRESSED(0, 190);
  if (left)
    *left = IsKeyDown(VK_NUMPAD4) || IsKeyDown(VK_LEFT) ||
            PAD::IS_DISABLED_CONTROL_PRESSED(0, 189);
}

// ============================================================
//  Shared per-frame + control helpers
// ============================================================

// Every menu suppresses the same phone / character-wheel controls so the
// player can't dismiss the menu sideways. Defined once here; every submenu
// calls it at the top of its draw loop.
static void DisableMenuPhoneControls() {
  CONTROLS::DISABLE_CONTROL_ACTION(0, 27, TRUE);   // INPUT_PHONE
  CONTROLS::DISABLE_CONTROL_ACTION(0, 172, TRUE);  // INPUT_PHONE_UP
  CONTROLS::DISABLE_CONTROL_ACTION(0, 173, TRUE);  // INPUT_PHONE_DOWN
  CONTROLS::DISABLE_CONTROL_ACTION(0, 174, TRUE);  // INPUT_PHONE_LEFT
  CONTROLS::DISABLE_CONTROL_ACTION(0, 175, TRUE);  // INPUT_PHONE_RIGHT
  CONTROLS::DISABLE_CONTROL_ACTION(0, 19, TRUE);   // INPUT_CHARACTER_WHEEL
  CONTROLS::DISABLE_CONTROL_ACTION(0, 166, TRUE);  // INPUT_SELECT_CHARACTER_MICHAEL
  CONTROLS::DISABLE_CONTROL_ACTION(0, 167, TRUE);  // INPUT_SELECT_CHARACTER_FRANKLIN
  CONTROLS::DISABLE_CONTROL_ACTION(0, 168, TRUE);  // INPUT_SELECT_CHARACTER_TREVOR
  CONTROLS::DISABLE_CONTROL_ACTION(0, 169, TRUE);  // INPUT_SELECT_CHARACTER_MULTIPLAYER
}

// Advance the world by exactly one frame's worth of plugin work while a menu
// is open. script.cpp's main loop is blocked inside ProcessConfigMenu, so the
// menu draw loops have to drive everything themselves. This mirrors the main
// loop's order precisely (sequence tick OR free-cam update, then time/weather,
// status text, global effects) so behavior is identical whether or not a menu
// happens to be open. Previously each submenu hand-rolled this and several
// forgot UpdateTimeWeather(), which froze time-lapse / weather-blend while you
// sat in the Lens or DoF submenu — this fixes that uniformly.
static void MenuFrameTick() {
  if (g_CameraMode == 1 && Sequence_IsInMode()) {
    Sequence_FrameTick();
  } else if (g_FreeCamActive) {
    UpdateFreeCamera();
  }
  UpdateAutoDrive();
  UpdateTimeWeather();
  UpdateStatusText();
  UpdateGlobalEffects();
}

// Fire a ray straight out of the camera and return the entity under the
// crosshair (0 if none). `flags` is the shapetest entity-type mask
// (30 = vehicles|peds|ragdolls|objects). Shared by every "lock onto whatever
// I'm aiming at" path in both the Free Camera and Sequence follow menus.
static int RaycastEntityFromCamera(int flags) {
  float posX, posY, posZ, pitch, yaw, roll;
  GetCameraState(posX, posY, posZ, pitch, yaw, roll);
  float yawRad = yaw * 0.0174532925f;
  float pitchRad = pitch * 0.0174532925f;
  float dirX = -sinf(yawRad) * cosf(pitchRad);
  float dirY = cosf(yawRad) * cosf(pitchRad);
  float dirZ = sinf(pitchRad);
  float endX = posX + dirX * 1000.0f;
  float endY = posY + dirY * 1000.0f;
  float endZ = posZ + dirZ * 1000.0f;
  int rayHandle = invoke<int>(0x377906D8A31E5586, posX, posY, posZ, endX, endY,
                              endZ, flags, PLAYER::PLAYER_PED_ID(), 7);
  int hit = 0, entityHit = 0;
  Vector3 ignore1{}, ignore2{};
  invoke<int>(0x3D87450E15D98694, rayHandle, &hit, &ignore1, &ignore2,
              &entityHit);
  if (hit && entityHit != 0 && ENTITY::DOES_ENTITY_EXIST(entityHit) &&
      (ENTITY::IS_ENTITY_A_PED(entityHit) ||
       ENTITY::IS_ENTITY_A_VEHICLE(entityHit) ||
       ENTITY::IS_ENTITY_AN_OBJECT(entityHit))) {
    return entityHit;
  }
  return 0;
}

// Draw the white hover marker floating above an entity — used while aiming,
// before the user commits the lock, so they can see what they'll grab.
static void DrawEntityHoverMarker(int entity) {
  if (entity == 0 || !ENTITY::DOES_ENTITY_EXIST(entity)) return;
  Vector3 entPos = ENTITY::GET_ENTITY_COORDS(entity, TRUE);
  GRAPHICS::DRAW_MARKER(0, entPos.x, entPos.y, entPos.z + 1.25f, 0, 0, 0, 0, 0,
                        0, 0.4f, 0.4f, 0.4f, 255, 255, 255, 200, TRUE, TRUE, 2,
                        FALSE, NULL, NULL, FALSE);
}

// Human-readable name for g_FollowMode (0/1/2). Single source of truth so the
// label reads the same in every menu that shows it.
static const char *FollowModeName(int mode) {
  return (mode == 1) ? "Player" : (mode == 2) ? "Aimed Entity" : "None";
}

// Adaptive auto-repeat for held left/right on numeric sliders. The longer the
// key is held the larger the step multiplier AND the faster the repeat, so a
// tap nudges by one base step while a long hold sweeps a wide range quickly.
// `held` is whether left or right is down this frame; `holdStart`/`wasHolding`
// are caller-owned state declared before the menu's loop. The chosen repeat
// delay (ms) is written to *waitOut; the return value is the multiplier to
// scale the row's base step by. Releasing the key resets the ramp, so repeated
// taps always move by exactly one base step (fine control preserved).
static float HoldAccel(bool held, DWORD &holdStart, bool &wasHolding,
                       DWORD *waitOut) {
  if (held && !wasHolding) holdStart = GetTickCount();
  wasHolding = held;
  if (!held) {
    if (waitOut) *waitOut = 100;
    return 1.0f;
  }
  DWORD elapsed = GetTickCount() - holdStart;
  float mult;
  DWORD wait;
  if      (elapsed > 3000) { mult = 10.0f; wait = 40; }
  else if (elapsed > 1500) { mult = 5.0f;  wait = 55; }
  else if (elapsed > 700)  { mult = 2.0f;  wait = 80; }
  else                     { mult = 1.0f;  wait = 110; }
  if (waitOut) *waitOut = wait;
  return mult;
}

// ============================================================
//  Formatting Helpers
// ============================================================

static std::string FormatBool(bool v) { return v ? "ON" : "OFF"; }

static std::string FormatFloat(float v, int decimals = 1) {
  char buf[32];
  if (decimals == 0)
    sprintf_s(buf, "%.0f", v);
  else if (decimals == 1)
    sprintf_s(buf, "%.1f", v);
  else
    sprintf_s(buf, "%.2f", v);
  return std::string(buf);
}

static std::string FormatInt(int v) {
  char buf[16];
  sprintf_s(buf, "%d", v);
  return std::string(buf);
}

static std::string FormatTime(int h, int m) {
  char buf[16];
  sprintf_s(buf, "%02d:%02d", h, m);
  return std::string(buf);
}

static bool PromptForFloat(float &outVal, float currentVal) {
  std::string valStr = FormatFloat(currentVal);
  GAMEPLAY::DISPLAY_ONSCREEN_KEYBOARD(1, (char *)"", (char *)"",
                                      (char *)valStr.c_str(), (char *)"",
                                      (char *)"", (char *)"", 15);
  while (GAMEPLAY::UPDATE_ONSCREEN_KEYBOARD() == 0) {
    MenuFrameTick();
    WAIT(0);
  }
  if (GAMEPLAY::UPDATE_ONSCREEN_KEYBOARD() == 1) {
    char *result = GAMEPLAY::GET_ONSCREEN_KEYBOARD_RESULT();
    if (result && result[0] != '\0') {
      outVal = (float)atof(result);
      return true;
    }
  }
  return false;
}

// ============================================================
//  Viewport scrolling for long submenus
// ============================================================
//
// Several submenus can grow taller than the screen: Movement Settings
// (Drone Mode + Aimed Entity follow can stack to ~20 rows) and the
// Pose Keyframes / Effect Events list menus (unbounded). Rather than
// shrink rows or omit options, we cap visible rows at
// kMaxVisibleListRows and slide a scroll window over the logical row
// range, keeping the selected row in view. The window auto-scrolls
// when the cursor would otherwise leave it; the title bar shows the
// visible range (e.g. "[5-16 / 32]") so the user always knows where
// they are.
//
// Declared here so every submenu below has the symbols in scope.

static const int kMaxVisibleListRows = 12;

// Adjusts `scrollOffset` so that `activeIdx` is visible within a
// window of `kMaxVisibleListRows` rows over a list of `lineCount` total
// rows. Idempotent — call before each frame's render pass.
static void EnsureRowVisible(int activeIdx, int lineCount, int &scrollOffset) {
  int maxOffset = lineCount - kMaxVisibleListRows;
  if (maxOffset < 0) maxOffset = 0;
  // Scroll up to bring the cursor into view from above
  if (activeIdx < scrollOffset) scrollOffset = activeIdx;
  // Scroll down to bring the cursor into view from below
  if (activeIdx >= scrollOffset + kMaxVisibleListRows)
    scrollOffset = activeIdx - kMaxVisibleListRows + 1;
  // Clamp final offset
  if (scrollOffset < 0) scrollOffset = 0;
  if (scrollOffset > maxOffset) scrollOffset = maxOffset;
}

// ============================================================
//  Sub-Menu: Movement Settings
// ============================================================

static void ProcessMovementMenu() {
  const float lineWidth = 300.0f;
  static bool s_LockFailed = false;
  int activeIdx = 0;
  // Movement Settings can balloon to ~20 rows when Drone Mode + Aimed
  // Entity follow are both active, which overflows the screen. We use
  // the same viewport-scrolling pattern as the Pose Keyframes / Effect
  // Events list menus: cap visible rows at kMaxVisibleListRows and slide
  // a window over the logical row range. scrollOffset survives loop
  // iterations so the viewport doesn't jump.
  int scrollOffset = 0;

  // Hold-to-accelerate state for left/right on numeric rows.
  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    // Dynamic line count
    int lineCount = 10; // Speed, Sensitivity, Zoom Speed, Roll Speed, Collision,
                        // Lock Height, Walk Mode, Acro Mode,
                        // Follow Target, Drone Mode
    if (g_WalkMode)
      lineCount += 1; // "Walk Height" only visible while Walk Mode is on
    if (g_FollowMode == 1) {
      lineCount += 1; // "Rigid Mode"
    } else if (g_FollowMode == 2) {
      lineCount += 1; // "Lock Entity"
      if (g_FollowTargetEntity != 0) {
        lineCount += 2; // "Show Marker" + "Rigid Mode"
      }
    }
    if (g_DroneMode)
      lineCount += 6; // Drone sub-options: Drag, Acc, Gravity, Banking, RotSmooth, FovSmooth

    if (activeIdx >= lineCount)
      activeIdx = lineCount - 1;
    EnsureRowVisible(activeIdx, lineCount, scrollOffset);

    int visEnd = scrollOffset + kMaxVisibleListRows;
    if (visEnd > lineCount) visEnd = lineCount;

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      // Title shows position when scrolled (X-Y / total) so the user
      // always knows where they are even when only a slice is rendered.
      char movTitle[64];
      if (lineCount > kMaxVisibleListRows) {
        sprintf_s(movTitle, "MOVEMENT SETTINGS  [%d-%d / %d]",
                  scrollOffset + 1, visEnd, lineCount);
      } else {
        sprintf_s(movTitle, "MOVEMENT SETTINGS");
      }
      DrawMenuLine(movTitle, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Per-row helpers — keep `row` incrementing as the logical index
      // (so activeIdx comparisons stay valid for input handling); only
      // call DrawMenuValue when the row is inside the visible window.
      // rowTop maps a logical row to its visible Y position by sliding
      // the viewport up by scrollOffset rows.
      auto vis = [&](int r) {
        return r >= scrollOffset && r < scrollOffset + kMaxVisibleListRows;
      };
      auto rowTop = [&](int r) { return 60.0f + (r - scrollOffset) * 36.0f; };

      int row = 0;
      if (vis(row))
        DrawMenuValue("Camera Speed", FormatFloat(g_CamSpeed), lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (vis(row))
        DrawMenuValue("Look Sensitivity", FormatFloat(g_CamSensitivity),
                      lineWidth, 9.0, rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (vis(row))
        DrawMenuValue("Zoom Speed", FormatFloat(g_ZoomSpeed), lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (vis(row))
        DrawMenuValue("Roll Speed", FormatFloat(g_RollSpeed), lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (vis(row))
        DrawMenuValue("World Collision", FormatBool(g_CamCollision), lineWidth,
                      9.0, rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (vis(row))
        DrawMenuValue("Lock Altitude", FormatBool(g_LockHeight), lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (vis(row))
        DrawMenuValue("Walk Mode", FormatBool(g_WalkMode), lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (g_WalkMode) {
        if (vis(row))
          DrawMenuValue("  - Walk Height (m)", FormatFloat(g_WalkHeight, 2),
                        lineWidth, 9.0, rowTop(row), 0.0, 9.0,
                        activeIdx == row);
        row++;
      }

      std::string rotEngineStr = g_RotationEngine ? "Acrobatic" : "Standard";
      if (vis(row))
        DrawMenuValue("Rotation Style", rotEngineStr, lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;

      // Follow Mode
      std::string followStr = FollowModeName(g_FollowMode);
      if (vis(row))
        DrawMenuValue("Follow Target", followStr, lineWidth, 9.0, rowTop(row),
                      0.0, 9.0, activeIdx == row);
      row++;

      if (g_FollowMode == 1) {
        if (vis(row))
          DrawMenuValue("  - Rigid Mode", FormatBool(g_FollowRigidMode),
                        lineWidth, 9.0, rowTop(row), 0.0, 9.0,
                        activeIdx == row);
        row++;
      } else if (g_FollowMode == 2) {
        std::string entityStr =
            (g_FollowTargetEntity == 0) ? "Click to Lock" : "Locked";

        if (vis(row))
          DrawMenuValue("  - Lock Entity", entityStr, lineWidth, 9.0,
                        rowTop(row), 0.0, 9.0, activeIdx == row);
        row++;

        if (g_FollowTargetEntity != 0) {
          if (vis(row))
            DrawMenuValue("  - Show Marker",
                          FormatBool(g_ShowLockedEntityMarker), lineWidth, 9.0,
                          rowTop(row), 0.0, 9.0, activeIdx == row);
          row++;

          if (vis(row))
            DrawMenuValue("  - Rigid Mode", FormatBool(g_FollowRigidMode),
                          lineWidth, 9.0, rowTop(row), 0.0, 9.0,
                          activeIdx == row);
          row++;
        }

        // Draw hover marker if we are not yet locked onto an entity
        if (g_FollowTargetEntity == 0) {
          DrawEntityHoverMarker(RaycastEntityFromCamera(30));
        }
      }

      std::string moveStyleStr = g_DroneMode ? "Drone" : "Standard";
      if (vis(row))
        DrawMenuValue("Movement Style", moveStyleStr, lineWidth, 9.0,
                      rowTop(row), 0.0, 9.0, activeIdx == row);
      row++;
      if (g_DroneMode) {
        if (vis(row))
          DrawMenuValue("  - Drag", FormatFloat(g_DroneDrag), lineWidth, 9.0,
                        rowTop(row), 0.0, 9.0, activeIdx == row);
        row++;
        if (vis(row))
          DrawMenuValue("  - Acceleration", FormatFloat(g_DroneAcceleration),
                        lineWidth, 9.0, rowTop(row), 0.0, 9.0,
                        activeIdx == row);
        row++;
        if (vis(row))
          DrawMenuValue("  - Gravity", FormatFloat(g_DroneGravity), lineWidth,
                        9.0, rowTop(row), 0.0, 9.0, activeIdx == row);
        row++;
        if (vis(row))
          DrawMenuValue("  - Banking", FormatFloat(g_DroneBanking), lineWidth,
                        9.0, rowTop(row), 0.0, 9.0, activeIdx == row);
        row++;
        if (vis(row))
          DrawMenuValue("  - Rot. Smoothing", FormatFloat(g_DroneRotSmoothing),
                        lineWidth, 9.0, rowTop(row), 0.0, 9.0,
                        activeIdx == row);
        row++;
        if (vis(row))
          DrawMenuValue("  - FOV Smoothing", FormatFloat(g_DroneFovSmoothing),
                        lineWidth, 9.0, rowTop(row), 0.0, 9.0,
                      activeIdx == row);
        row++;
      }

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    int collisionIdx = 4;
    int lockHeightIdx = 5;
    int walkModeIdx = 6;
    // Walk Height row is hidden when Walk Mode is off — everything below
    // shifts up by one in that case.
    int walkHeightIdx = g_WalkMode ? 7 : -1;
    int rotationIdx = g_WalkMode ? 8 : 7;
    int followIdx = g_WalkMode ? 9 : 8;

    int lockIdx = -1;
    int markerIdx = -1;
    int rigidIdx = -1;
    int droneToggleIdx = followIdx + 1;

    if (g_FollowMode == 1) {
      rigidIdx = droneToggleIdx;
      droneToggleIdx++;
    } else if (g_FollowMode == 2) {
      lockIdx = droneToggleIdx;
      droneToggleIdx++;
      if (g_FollowTargetEntity != 0) {
        markerIdx = droneToggleIdx;
        rigidIdx = droneToggleIdx + 1;
        droneToggleIdx += 2;
      }
    }

    int droneStartIdx = g_DroneMode ? (droneToggleIdx + 1) : -1;

    if (bSelect) {
      if (activeIdx == collisionIdx) {
        MenuBeep();
        g_CamCollision = !g_CamCollision;
        SetStatusText(g_CamCollision ? "Collision enabled"
                                     : "Collision disabled");
        waitTime = 200;
      } else if (activeIdx == lockHeightIdx) {
        MenuBeep();
        g_LockHeight = !g_LockHeight;
        SetStatusText(g_LockHeight ? "Altitude locked (Z-axis frozen)"
                                   : "Altitude unlocked");
        waitTime = 200;
      } else if (activeIdx == walkModeIdx) {
        MenuBeep();
        g_WalkMode = !g_WalkMode;
        SetStatusText(g_WalkMode
                          ? "Walk mode enabled — camera locked to ground"
                          : "Walk mode disabled");
        waitTime = 200;
      } else if (activeIdx == walkHeightIdx) {
        MenuBeep();
        float newVal;
        if (PromptForFloat(newVal, g_WalkHeight)) {
          g_WalkHeight = newVal;
          if (g_WalkHeight < 0.1f) g_WalkHeight = 0.1f;
          if (g_WalkHeight > 50.0f) g_WalkHeight = 50.0f;
        }
        waitTime = 200;
      } else if (activeIdx == rotationIdx) {
        MenuBeep();
        g_RotationEngine = !g_RotationEngine;
        SetStatusText(g_RotationEngine ? "Quaternion engine active (Acrobatic)"
                                       : "Euler engine active");
        waitTime = 200;
      } else if (activeIdx == droneToggleIdx) {
        MenuBeep();
        g_DroneMode = !g_DroneMode;
        SetStatusText(g_DroneMode ? "Drone mode enabled"
                                  : "Drone mode disabled");
        waitTime = 200;
      } else if (activeIdx == lockIdx) {
        MenuBeep();
        if (g_FollowTargetEntity != 0) {
          g_FollowTargetEntity = 0;
          SetStatusText("Entity unlocked");
          waitTime = 200;
        } else {
          // Fire a raycast forward to lock the target at center of screen
          int e = RaycastEntityFromCamera(30);
          if (e != 0) {
            g_FollowTargetEntity = e;
            s_LockFailed = false;
            SetStatusText("Locked onto entity");
          } else {
            g_FollowTargetEntity = 0;
            s_LockFailed = true;
            SetStatusText("No entity found. Aim closer.");
          }
          waitTime = 200;
        }
      } else if (markerIdx != -1 && activeIdx == markerIdx) {
        MenuBeep();
        g_ShowLockedEntityMarker = !g_ShowLockedEntityMarker;
        SetStatusText(g_ShowLockedEntityMarker ? "Locked marker visible"
                                               : "Locked marker hidden");
        waitTime = 200;
      } else if (rigidIdx != -1 && activeIdx == rigidIdx) {
        MenuBeep();
        g_FollowRigidMode = !g_FollowRigidMode;
        SetStatusText(g_FollowRigidMode ? "Rigid mode enabled"
                                        : "Rigid mode disabled");
        waitTime = 200;
      }
    }

    // Hold-to-accelerate applies only to the continuous numeric rows; the
    // Follow-mode cycle and the marker/rigid toggles keep a fixed repeat so
    // they don't spin when held.
    bool numericRow = (activeIdx >= 0 && activeIdx <= 3) ||
                      (walkHeightIdx != -1 && activeIdx == walkHeightIdx) ||
                      (droneStartIdx != -1 && activeIdx >= droneStartIdx);
    DWORD adjWait = 100;
    float mult = HoldAccel((bRight || bLeft) && numericRow, holdStart,
                           wasHolding, &adjWait);

    if (bRight) {
      MenuBeep();
      if (activeIdx == 0) {
        g_CamSpeed += 0.1f * mult;
        if (g_CamSpeed > 50.0f)
          g_CamSpeed = 50.0f;
      } else if (activeIdx == 1) {
        g_CamSensitivity += 0.1f * mult;
        if (g_CamSensitivity > 5.0f)
          g_CamSensitivity = 5.0f;
      } else if (activeIdx == 2) {
        g_ZoomSpeed += 0.1f * mult;
        if (g_ZoomSpeed > 10.0f)
          g_ZoomSpeed = 10.0f;
      } else if (activeIdx == 3) {
        g_RollSpeed += 0.1f * mult;
        if (g_RollSpeed > 10.0f)
          g_RollSpeed = 10.0f;
      } else if (activeIdx == walkHeightIdx) {
        g_WalkHeight += 0.1f * mult;
        if (g_WalkHeight > 50.0f)
          g_WalkHeight = 50.0f;
      } else if (activeIdx == followIdx) {
        g_FollowMode = (g_FollowMode + 1) % 3;
        if (g_FollowMode == 1 && g_MovePlayerWithCamera) {
          g_FollowMode = 2; // Skip Player follow if Move Player is active
        }
        if (g_FollowMode != 2)
          g_FollowTargetEntity = 0;
      } else if (markerIdx != -1 && activeIdx == markerIdx) {
        g_ShowLockedEntityMarker = !g_ShowLockedEntityMarker;
      } else if (rigidIdx != -1 && activeIdx == rigidIdx) {
        g_FollowRigidMode = !g_FollowRigidMode;
      } else if (droneStartIdx != -1 && activeIdx >= droneStartIdx) {
        int dIdx = activeIdx - droneStartIdx;
        switch (dIdx) {
        case 0:
          g_DroneDrag += 0.5f * mult;
          if (g_DroneDrag > 20.0f)
            g_DroneDrag = 20.0f;
          break;
        case 1:
          g_DroneAcceleration += 1.0f * mult;
          if (g_DroneAcceleration > 50.0f)
            g_DroneAcceleration = 50.0f;
          break;
        case 2:
          g_DroneGravity += 0.5f * mult;
          if (g_DroneGravity > 20.0f)
            g_DroneGravity = 20.0f;
          break;
        case 3:
          g_DroneBanking += 1.0f * mult;
          if (g_DroneBanking > 45.0f)
            g_DroneBanking = 45.0f;
          break;
        case 4:
          g_DroneRotSmoothing += 0.5f * mult;
          if (g_DroneRotSmoothing > 20.0f)
            g_DroneRotSmoothing = 20.0f;
          break;
        case 5:
          g_DroneFovSmoothing += 0.5f * mult;
          if (g_DroneFovSmoothing > 20.0f)
            g_DroneFovSmoothing = 20.0f;
          break;
        }
      }
      waitTime = adjWait;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 0) {
        g_CamSpeed -= 0.1f * mult;
        if (g_CamSpeed < 0.1f)
          g_CamSpeed = 0.1f;
      } else if (activeIdx == 1) {
        g_CamSensitivity -= 0.1f * mult;
        if (g_CamSensitivity < 0.1f)
          g_CamSensitivity = 0.1f;
      } else if (activeIdx == 2) {
        g_ZoomSpeed -= 0.1f * mult;
        if (g_ZoomSpeed < 0.1f)
          g_ZoomSpeed = 0.1f;
      } else if (activeIdx == 3) {
        g_RollSpeed -= 0.1f * mult;
        if (g_RollSpeed < 0.1f)
          g_RollSpeed = 0.1f;
      } else if (activeIdx == walkHeightIdx) {
        g_WalkHeight -= 0.1f * mult;
        if (g_WalkHeight < 0.1f)
          g_WalkHeight = 0.1f;
      } else if (activeIdx == followIdx) {
        g_FollowMode = (g_FollowMode == 0) ? 2 : g_FollowMode - 1;
        if (g_FollowMode == 1 && g_MovePlayerWithCamera) {
          g_FollowMode =
              0; // Skip Player follow backwards if Move Player is active
        }
        if (g_FollowMode != 2)
          g_FollowTargetEntity = 0;
      } else if (markerIdx != -1 && activeIdx == markerIdx) {
        g_ShowLockedEntityMarker = !g_ShowLockedEntityMarker;
      } else if (rigidIdx != -1 && activeIdx == rigidIdx) {
        g_FollowRigidMode = !g_FollowRigidMode;
      } else if (droneStartIdx != -1 && activeIdx >= droneStartIdx) {
        int dIdx = activeIdx - droneStartIdx;
        switch (dIdx) {
        case 0:
          g_DroneDrag -= 0.5f * mult;
          if (g_DroneDrag < 0.5f)
            g_DroneDrag = 0.5f;
          break;
        case 1:
          g_DroneAcceleration -= 1.0f * mult;
          if (g_DroneAcceleration < 1.0f)
            g_DroneAcceleration = 1.0f;
          break;
        case 2:
          g_DroneGravity -= 0.5f * mult;
          if (g_DroneGravity < 0.0f)
            g_DroneGravity = 0.0f;
          break;
        case 3:
          g_DroneBanking -= 1.0f * mult;
          if (g_DroneBanking < 0.0f)
            g_DroneBanking = 0.0f;
          break;
        case 4:
          g_DroneRotSmoothing -= 0.5f * mult;
          if (g_DroneRotSmoothing < 1.0f)
            g_DroneRotSmoothing = 1.0f;
          break;
        case 5:
          g_DroneFovSmoothing -= 0.5f * mult;
          if (g_DroneFovSmoothing < 0.0f)
            g_DroneFovSmoothing = 0.0f;
          break;
        }
      }
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Sub-Menu: Lens Settings
// ============================================================

static void ProcessLensMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 2;
  int activeIdx = 0;

  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      DrawMenuLine("LENS SETTINGS", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Lens Zoom (FOV)", FormatFloat(g_CamFOV, 0) + "\xC2\xB0",
                    lineWidth, 9.0, 60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Lens Tilt (Roll)", FormatFloat(g_CamRoll, 0) + "\xC2\xB0",
                    lineWidth, 9.0, 60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == 0) {
        float newVal;
        if (PromptForFloat(newVal, g_CamFOV)) {
          g_CamFOV = newVal;
          if (g_CamFOV < 5.0f)
            g_CamFOV = 5.0f;
          if (g_CamFOV > 130.0f)
            g_CamFOV = 130.0f;
        }
      } else if (activeIdx == 1) {
        float newVal;
        if (PromptForFloat(newVal, g_CamRoll)) {
          g_CamRoll = newVal;
          while (g_CamRoll > 180.0f)
            g_CamRoll -= 360.0f;
          while (g_CamRoll < -180.0f)
            g_CamRoll += 360.0f;
        }
      }
      waitTime = 200;
    }

    DWORD adjWait = 100;
    float mult = HoldAccel(bRight || bLeft, holdStart, wasHolding, &adjWait);

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        g_CamFOV += 1.0f * mult;
        if (g_CamFOV > 130.0f)
          g_CamFOV = 130.0f;
        break;
      case 1:
        g_CamRoll += 1.0f * mult;
        if (g_CamRoll > 180.0f)
          g_CamRoll -= 360.0f;
        break;
      }
      waitTime = adjWait;
    }

    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        g_CamFOV -= 1.0f * mult;
        if (g_CamFOV < 5.0f)
          g_CamFOV = 5.0f;
        break;
      case 1:
        g_CamRoll -= 1.0f * mult;
        if (g_CamRoll < -180.0f)
          g_CamRoll += 360.0f;
        break;
      }
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Sub-Menu: Depth of Field
// ============================================================

static void ProcessDoFMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 5;
  int activeIdx = 0;

  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      DrawMenuLine("DEPTH OF FIELD", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Depth of Field", FormatBool(g_DoFEnabled), lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Auto-Focus", FormatBool(g_DoFAutofocus), lineWidth, 9.0,
                    60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      DrawMenuValue("Manual Focus Dist.", FormatFloat(g_DoFFocusDist), lineWidth,
                    9.0, 60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Near Focus Range", FormatFloat(g_DoFMaxNearInFocus),
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Far Focus Range", FormatFloat(g_DoFMaxFarInFocus), lineWidth,
                    9.0, 60.0 + 4 * 36.0, 0.0, 9.0, activeIdx == 4);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      if (activeIdx == 0) {
        MenuBeep();
        g_DoFEnabled = !g_DoFEnabled;
        SetStatusText(g_DoFEnabled ? "DoF enabled" : "DoF disabled");
        waitTime = 200;
      } else if (activeIdx == 1) {
        MenuBeep();
        g_DoFAutofocus = !g_DoFAutofocus;
        SetStatusText(g_DoFAutofocus ? "Autofocus enabled"
                                     : "Autofocus disabled");
        waitTime = 200;
      } else if (activeIdx == 2) {
        MenuBeep();
        float newVal;
        if (PromptForFloat(newVal, g_DoFFocusDist)) {
          g_DoFFocusDist = newVal;
          if (g_DoFFocusDist > 500.0f)
            g_DoFFocusDist = 500.0f;
          if (g_DoFFocusDist < 0.5f)
            g_DoFFocusDist = 0.5f;
        }
        waitTime = 200;
      } else if (activeIdx == 3) {
        MenuBeep();
        float newVal;
        if (PromptForFloat(newVal, g_DoFMaxNearInFocus)) {
          g_DoFMaxNearInFocus = newVal;
          if (g_DoFMaxNearInFocus > 50.0f)
            g_DoFMaxNearInFocus = 50.0f;
          if (g_DoFMaxNearInFocus < 0.0f)
            g_DoFMaxNearInFocus = 0.0f;
        }
        waitTime = 200;
      } else if (activeIdx == 4) {
        MenuBeep();
        float newVal;
        if (PromptForFloat(newVal, g_DoFMaxFarInFocus)) {
          g_DoFMaxFarInFocus = newVal;
          if (g_DoFMaxFarInFocus > 50.0f)
            g_DoFMaxFarInFocus = 50.0f;
          if (g_DoFMaxFarInFocus < 0.0f)
            g_DoFMaxFarInFocus = 0.0f;
        }
        waitTime = 200;
      }
    }

    DWORD adjWait = 100;
    float mult = HoldAccel(bRight || bLeft, holdStart, wasHolding, &adjWait);

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 2:
        g_DoFFocusDist += 1.0f * mult;
        if (g_DoFFocusDist > 500.0f)
          g_DoFFocusDist = 500.0f;
        break;
      case 3:
        g_DoFMaxNearInFocus += 0.1f * mult;
        if (g_DoFMaxNearInFocus > 50.0f)
          g_DoFMaxNearInFocus = 50.0f;
        break;
      case 4:
        g_DoFMaxFarInFocus += 0.1f * mult;
        if (g_DoFMaxFarInFocus > 50.0f)
          g_DoFMaxFarInFocus = 50.0f;
        break;
      }
      waitTime = adjWait;
    }
    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 2:
        g_DoFFocusDist -= 1.0f * mult;
        if (g_DoFFocusDist < 0.5f)
          g_DoFFocusDist = 0.5f;
        break;
      case 3:
        g_DoFMaxNearInFocus -= 0.1f * mult;
        if (g_DoFMaxNearInFocus < 0.0f)
          g_DoFMaxNearInFocus = 0.0f;
        break;
      case 4:
        g_DoFMaxFarInFocus -= 0.1f * mult;
        if (g_DoFMaxFarInFocus < 0.0f)
          g_DoFMaxFarInFocus = 0.0f;
        break;
      }
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Sub-Menu: Time & Weather
// ============================================================

static void ProcessTimeWeatherMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 10;
  int activeIdx = 0;

  // Hold-to-accelerate tracking for time-of-day left/right
  DWORD timeHoldStart = 0;   // GetTickCount() when the button was first held
  bool  wasHoldingTime = false;  // was left/right held on previous iteration?
  // Separate hold-to-accelerate state for the Weather Blend row.
  DWORD blendHoldStart = 0;
  bool  wasHoldingBlend = false;
  // ...and for the Slow Motion (time scale) row.
  DWORD scaleHoldStart = 0;
  bool  wasHoldingScale = false;

  // Sync our variables with the current game time right when we open the menu
  // so the UI always reflects reality, instead of reading it every frame.
  if (!g_TimePaused) {
    g_TimeHour = TIME::GET_CLOCK_HOURS();
    g_TimeMinute = TIME::GET_CLOCK_MINUTES();
  }

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      DrawMenuLine("TIME & WEATHER", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Pause Time of Day", FormatBool(g_TimePaused), lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Time of Day", FormatTime(g_TimeHour, g_TimeMinute), lineWidth,
                    9.0, 60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);

      std::string lapseStr = "Off";
      if (g_TimelapseMode == 1)
        lapseStr = "Slow";
      else if (g_TimelapseMode == 2)
        lapseStr = "Medium";
      else if (g_TimelapseMode == 3)
        lapseStr = "Fast";

      DrawMenuValue("Time-lapse Speed", lapseStr, lineWidth, 9.0, 60.0 + 2 * 36.0, 0.0,
                    9.0, activeIdx == 2);

      DrawMenuValue("Primary Weather", std::string(g_WeatherNames[g_Weather1Index]),
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Secondary Weather", std::string(g_WeatherNames[g_Weather2Index]),
                    lineWidth, 9.0, 60.0 + 4 * 36.0, 0.0, 9.0, activeIdx == 4);
      char blendStr[32];
      sprintf_s(blendStr, "%d%%", (int)(g_WeatherBlend * 100.0f));
      DrawMenuValue("Weather Blend Ratio", std::string(blendStr), lineWidth, 9.0,
                    60.0 + 5 * 36.0, 0.0, 9.0, activeIdx == 5);
      DrawMenuValue("Apply Weather", "", lineWidth, 9.0, 60.0 + 6 * 36.0, 0.0,
                    9.0, activeIdx == 6);
      DrawMenuValue("Pause Game", FormatBool(g_FreezeWorld), lineWidth, 9.0,
                    60.0 + 7 * 36.0, 0.0, 9.0, activeIdx == 7);
      DrawMenuValue("Freeze All Entities", FormatBool(g_FreezeEntities), lineWidth,
                    9.0, 60.0 + 8 * 36.0, 0.0, 9.0, activeIdx == 8);
      // Slow Motion — Pause Game overrides it (shows "paused"). 100% = real
      // time. Free Camera only.
      char slowStr[24];
      if (g_FreezeWorld)
        sprintf_s(slowStr, "paused");
      else
        sprintf_s(slowStr, "%d%%", (int)(g_WorldTimeScale * 100.0f + 0.5f));
      DrawMenuValue("Slow Motion", std::string(slowStr), lineWidth, 9.0,
                    60.0 + 9 * 36.0, 0.0, 9.0, activeIdx == 9);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      if (activeIdx == 0) {
        MenuBeep();
        g_TimePaused = !g_TimePaused;
        SetStatusText(g_TimePaused ? "Time paused" : "Time resumed");
        waitTime = 200;
      } else if (activeIdx == 1) {
        MenuBeep();
        float newVal;
        if (PromptForFloat(newVal, g_TimeHour)) {
          g_TimeHour = (int)newVal;
          if (g_TimeHour > 23)
            g_TimeHour = 23;
          if (g_TimeHour < 0)
            g_TimeHour = 0;

          if (!g_TimePaused) {
            SetClockTime(g_TimeHour, g_TimeMinute, 0);
          }
        }
        waitTime = 200;
      } else if (activeIdx == 2) {
        MenuBeep();
        g_TimelapseMode = (g_TimelapseMode + 1) % 4;
        waitTime = 200;
      } else if (activeIdx == 5) {
        MenuBeep();
        float newVal;
        if (PromptForFloat(newVal, g_WeatherBlend * 100.0f)) {
          g_WeatherBlend = newVal / 100.0f;
          if (g_WeatherBlend > 1.0f)
            g_WeatherBlend = 1.0f;
          if (g_WeatherBlend < 0.0f)
            g_WeatherBlend = 0.0f;
        }
        waitTime = 200;
      } else if (activeIdx == 6) {
        MenuBeep();
        if (g_WeatherBlend <= 0.0f) {
          g_BlendWeatherActive = false;
          GAMEPLAY::SET_WEATHER_TYPE_NOW_PERSIST(
              (char *)g_WeatherNames[g_Weather1Index]);
          SetStatusText(std::string("Weather: ") +
                        g_WeatherNames[g_Weather1Index]);
        } else {
          g_BlendWeatherActive = true;
          SetStatusText(std::string("Blended: ") +
                        g_WeatherNames[g_Weather1Index] + " | " +
                        g_WeatherNames[g_Weather2Index]);
        }
        waitTime = 200;
      } else if (activeIdx == 7) {
        MenuBeep();
        g_FreezeWorld = !g_FreezeWorld;
        SetStatusText(g_FreezeWorld ? "Game paused (full freeze)"
                                    : "Game resumed");
        waitTime = 200;
      } else if (activeIdx == 8) {
        MenuBeep();
        g_FreezeEntities = !g_FreezeEntities;
        SetStatusText(g_FreezeEntities ? "Entities frozen (camera stays live)"
                                       : "Entities released");
        waitTime = 200;
      } else if (activeIdx == 9) {
        MenuBeep();
        g_WorldTimeScale = 1.0f; // Enter resets slow motion to real time
        SetStatusText("Slow motion reset to 100%");
        waitTime = 200;
      }
    }

    // --- Determine hold acceleration for time-of-day ---
    bool holdingTimeNow = (bRight || bLeft) && (activeIdx == 1);
    if (holdingTimeNow && !wasHoldingTime)
      timeHoldStart = GetTickCount(); // first frame of hold
    wasHoldingTime = holdingTimeNow;

    // Compute step size & repeat delay based on hold duration
    int  timeStep  = 1;   // minutes per tick
    DWORD timeWait = 150; // ms between repeats
    if (holdingTimeNow) {
      DWORD held = GetTickCount() - timeHoldStart;
      if (held > 3000)      { timeStep = 15; timeWait = 40; }
      else if (held > 1500) { timeStep = 5;  timeWait = 60; }
      else if (held > 500)  { timeStep = 1;  timeWait = 80; }
      // else default: 1 min, 150 ms
    }

    // Weather Blend (row 5) gets its own hold-to-accelerate ramp. The Time
    // row keeps its bespoke ramp above; enum rows (timelapse / weather) stay
    // at a fixed repeat so they don't spin through the list when held.
    DWORD blendWait = 100;
    float blendMult = HoldAccel((bRight || bLeft) && activeIdx == 5,
                                blendHoldStart, wasHoldingBlend, &blendWait);
    // Slow Motion (row 9) ramp.
    DWORD scaleWait = 100;
    float scaleMult = HoldAccel((bRight || bLeft) && activeIdx == 9,
                                scaleHoldStart, wasHoldingScale, &scaleWait);

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 1: {
        int totalMinutes = g_TimeHour * 60 + g_TimeMinute + timeStep;
        totalMinutes %= (24 * 60);
        g_TimeHour   = totalMinutes / 60;
        g_TimeMinute = totalMinutes % 60;
        if (!g_TimePaused) {
          SetClockTime(g_TimeHour, g_TimeMinute, 0);
        }
        waitTime = timeWait;
      } break;
      case 2:
        g_TimelapseMode = (g_TimelapseMode + 1) % 4;
        break;
      case 3:
        g_Weather1Index = (g_Weather1Index + 1) % g_WeatherCount;
        break;
      case 4:
        g_Weather2Index = (g_Weather2Index + 1) % g_WeatherCount;
        break;
      case 5:
        g_WeatherBlend += 0.05f * blendMult;
        if (g_WeatherBlend > 1.0f)
          g_WeatherBlend = 1.0f;
        break;
      case 9:
        g_WorldTimeScale += 0.01f * scaleMult;
        if (g_WorldTimeScale > 1.0f)
          g_WorldTimeScale = 1.0f;
        break;
      }
      if (activeIdx == 5)
        waitTime = blendWait;
      else if (activeIdx == 9)
        waitTime = scaleWait;
      else if (activeIdx != 1)
        waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 1: {
        int totalMinutes = g_TimeHour * 60 + g_TimeMinute - timeStep;
        if (totalMinutes < 0)
          totalMinutes += 24 * 60;
        g_TimeHour   = totalMinutes / 60;
        g_TimeMinute = totalMinutes % 60;
        if (!g_TimePaused) {
          SetClockTime(g_TimeHour, g_TimeMinute, 0);
        }
        waitTime = timeWait;
      } break;
      case 2:
        g_TimelapseMode = (g_TimelapseMode == 0) ? 3 : g_TimelapseMode - 1;
        break;
      case 3:
        g_Weather1Index =
            (g_Weather1Index == 0) ? g_WeatherCount - 1 : g_Weather1Index - 1;
        break;
      case 4:
        g_Weather2Index =
            (g_Weather2Index == 0) ? g_WeatherCount - 1 : g_Weather2Index - 1;
        break;
      case 5:
        g_WeatherBlend -= 0.05f * blendMult;
        if (g_WeatherBlend < 0.0f)
          g_WeatherBlend = 0.0f;
        break;
      case 9:
        g_WorldTimeScale -= 0.01f * scaleMult;
        if (g_WorldTimeScale < 0.01f)
          g_WorldTimeScale = 0.01f;
        break;
      }
      if (activeIdx == 5)
        waitTime = blendWait;
      else if (activeIdx == 9)
        waitTime = scaleWait;
      else if (activeIdx != 1)
        waitTime = 100;
    }
  }
}

// ============================================================
//  Sub-Menu: Misc Settings
// ============================================================

static void ProcessMiscMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 6;
  int activeIdx = 0;

  // Misc now holds only Free Camera *mechanics* — how the flycam relates to
  // the player ped. World/scene toggles (HUD, weather, etc.) live under
  // "World & Scene". Misc is only reachable from the Free Camera menu, where
  // the flycam is always engaged, so no freecam-active guards are needed.
  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      DrawMenuLine("MISC SETTINGS", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Move Player with Camera", FormatBool(g_MovePlayerWithCamera),
                    lineWidth, 9.0, 60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Save Position on Exit", FormatBool(g_RememberCamPosition),
                    lineWidth, 9.0, 60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      DrawMenuValue("Lock Camera Position", FormatBool(g_LockCamera), lineWidth,
                    9.0, 60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Allow Player to Move", FormatBool(g_EnablePlayerMovement),
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Snap Camera to Player", "Press Enter", lineWidth, 9.0,
                    60.0 + 4 * 36.0, 0.0, 9.0, activeIdx == 4);
      DrawMenuValue("Level Horizon (reset roll)", "Press Enter", lineWidth, 9.0,
                    60.0 + 5 * 36.0, 0.0, 9.0, activeIdx == 5);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, NULL, NULL);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        g_MovePlayerWithCamera = !g_MovePlayerWithCamera;
        if (g_MovePlayerWithCamera && g_FollowMode == 1) {
          g_FollowMode = 0; // Prevent getting stuck in recursive lock
        }
        SetStatusText(g_MovePlayerWithCamera ? "Player follows camera"
                                             : "Player stay fixed");
        break;
      case 1:
        g_RememberCamPosition = !g_RememberCamPosition;
        SetStatusText(g_RememberCamPosition ? "Camera position remembered"
                                            : "Camera resets on toggle");
        break;
      case 2:
        g_LockCamera = !g_LockCamera;
        SetStatusText(g_LockCamera ? "Camera locked" : "Camera unlocked");
        break;
      case 3:
        g_EnablePlayerMovement = !g_EnablePlayerMovement;
        {
          Ped ped = PLAYER::PLAYER_PED_ID();
          if (g_EnablePlayerMovement) {
            ENTITY::FREEZE_ENTITY_POSITION(ped, FALSE);
            ENTITY::SET_ENTITY_COLLISION(ped, TRUE, FALSE);
          } else {
            ENTITY::FREEZE_ENTITY_POSITION(ped, TRUE);
            ENTITY::SET_ENTITY_COLLISION(ped, FALSE, FALSE);
          }
        }
        SetStatusText(g_EnablePlayerMovement ? "Player movement enabled"
                                             : "Player movement disabled");
        break;
      case 4:
        SnapCameraToPlayer();
        SetStatusText("Camera snapped to player");
        break;
      case 5:
        LevelCameraHorizon();
        SetStatusText("Horizon leveled (roll reset)");
        break;
      }
      waitTime = 200;
    }
  }
}

// ============================================================
//  Sub-Menu: World & Scene
// ============================================================
//
// Shared hub for everything that isn't the camera itself — time, weather,
// world freeze, HUD / player visibility, and the global Save Settings action.
// Reachable from BOTH the Free Camera menu and the Camera Sequence menu so
// you can dress the scene without leaving whichever camera you're driving.
// Time & Weather is kept as its own sub-submenu (it's a dense screen with its
// own hold-to-scrub time logic) rather than inlined here.

// Defined further down (just before the Free Camera main menu). Forward-
// declared so World & Scene — shared by Free Camera and Camera Sequence —
// can open it from both modes.
static void ProcessAutoDriveMenu();

static void ProcessWorldSceneMenu() {
  const float lineWidth = 320.0f;
  const int lineCount = 8;
  int activeIdx = 0;
  // "Reset to Defaults" arms on first Enter and commits on a second within a
  // few seconds, so a stray click can't wipe a dialed-in setup.
  DWORD resetArmedUntil = 0;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      DrawMenuLine("WORLD & SCENE", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Time & Weather...", "", lineWidth, 9.0, 60.0 + 0 * 36.0,
                    0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Auto Drive...",
                    g_AutoDriveEnabled
                        ? (g_AutoDriveMode == 0 ? "On - waypoint" : "On - wander")
                        : "Off",
                    lineWidth, 9.0, 60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      DrawMenuValue("Hide Game HUD", FormatBool(g_HideHUD), lineWidth, 9.0,
                    60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Hide Player Character", FormatBool(g_HidePlayer), lineWidth,
                    9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Disable Vehicle Shake", FormatBool(g_DisableVehicleShake),
                    lineWidth, 9.0, 60.0 + 4 * 36.0, 0.0, 9.0, activeIdx == 4);
      DrawMenuValue("Show Info Overlay", FormatBool(g_ShowInfoOverlay), lineWidth,
                    9.0, 60.0 + 5 * 36.0, 0.0, 9.0, activeIdx == 5);
      DrawMenuValue("Save Settings to INI", "Press Enter", lineWidth, 9.0,
                    60.0 + 6 * 36.0, 0.0, 9.0, activeIdx == 6);
      DrawMenuValue("Reset to Defaults",
                    (GetTickCount() < resetArmedUntil) ? "Press again to confirm"
                                                       : "Press Enter",
                    lineWidth, 9.0, 60.0 + 7 * 36.0, 0.0, 9.0, activeIdx == 7);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, NULL, NULL);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        ProcessTimeWeatherMenu();
        break;
      case 1:
        ProcessAutoDriveMenu();
        break;
      case 2:
        g_HideHUD = !g_HideHUD;
        SetStatusText(g_HideHUD ? "HUD Hidden" : "HUD Visible");
        break;
      case 3:
        g_HidePlayer = !g_HidePlayer;
        SetStatusText(g_HidePlayer ? "Player Hidden" : "Player Visible");
        break;
      case 4:
        g_DisableVehicleShake = !g_DisableVehicleShake;
        SetStatusText(g_DisableVehicleShake ? "Vehicle shake disabled"
                                            : "Vehicle shake enabled");
        break;
      case 5:
        g_ShowInfoOverlay = !g_ShowInfoOverlay;
        SetStatusText(g_ShowInfoOverlay ? "Info overlay shown"
                                        : "Info overlay hidden");
        break;
      case 6:
        SaveSettings();
        SetStatusText("Settings saved to SimpleCamera.ini");
        break;
      case 7:
        if (GetTickCount() < resetArmedUntil) {
          ResetSettingsToDefaults();
          resetArmedUntil = 0;
          SetStatusText("Settings reset to defaults");
        } else {
          resetArmedUntil = GetTickCount() + 3000;
          SetStatusText("Press Enter again to confirm reset");
        }
        break;
      }
      waitTime = 200;
    }
  }
}

// ============================================================
//  Sub-Menu: Camera Effects (Procedural Shake)
// ============================================================

static const char *kShakePresetNames[6] = {
    "Off", "Subtle", "Handheld", "Vehicle", "Earthquake", "Custom"};

static void MarkShakeCustom() { g_ShakePreset = 5; }

static void ClampShake() {
  if (g_ShakeAmp < 0.0f) g_ShakeAmp = 0.0f;
  if (g_ShakeAmp > 3.0f) g_ShakeAmp = 3.0f;
  if (g_ShakeFreq < 0.05f) g_ShakeFreq = 0.05f;
  if (g_ShakeFreq > 20.0f) g_ShakeFreq = 20.0f;
  if (g_ShakeSpeedAmpCoupling < 0.0f) g_ShakeSpeedAmpCoupling = 0.0f;
  if (g_ShakeSpeedAmpCoupling > 2.0f) g_ShakeSpeedAmpCoupling = 2.0f;
  if (g_ShakeSpeedFreqCoupling < 0.0f) g_ShakeSpeedFreqCoupling = 0.0f;
  if (g_ShakeSpeedFreqCoupling > 2.0f) g_ShakeSpeedFreqCoupling = 2.0f;
  if (g_ShakeRotWeight < 0.0f) g_ShakeRotWeight = 0.0f;
  if (g_ShakeRotWeight > 2.0f) g_ShakeRotWeight = 2.0f;
  if (g_ShakePosWeight < 0.0f) g_ShakePosWeight = 0.0f;
  if (g_ShakePosWeight > 2.0f) g_ShakePosWeight = 2.0f;
}

static void ProcessCameraEffectsMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 10;
  int activeIdx = 0;

  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();

      DrawMenuLine("CAMERA EFFECTS", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);

      int presetIdx = (g_ShakePreset >= 0 && g_ShakePreset <= 5)
                          ? g_ShakePreset
                          : 5;
      DrawMenuValue("Enabled", FormatBool(g_ShakeEnabled), lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Preset", kShakePresetNames[presetIdx], lineWidth, 9.0,
                    60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      DrawMenuValue("Base Amplitude", FormatFloat(g_ShakeAmp, 2), lineWidth, 9.0,
                    60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Base Frequency (Hz)", FormatFloat(g_ShakeFreq, 2),
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Speed -> Amplitude",
                    FormatFloat(g_ShakeSpeedAmpCoupling, 2), lineWidth, 9.0,
                    60.0 + 4 * 36.0, 0.0, 9.0, activeIdx == 4);
      DrawMenuValue("Speed -> Frequency",
                    FormatFloat(g_ShakeSpeedFreqCoupling, 2), lineWidth, 9.0,
                    60.0 + 5 * 36.0, 0.0, 9.0, activeIdx == 5);
      DrawMenuValue("Rotation Weight", FormatFloat(g_ShakeRotWeight, 2),
                    lineWidth, 9.0, 60.0 + 6 * 36.0, 0.0, 9.0, activeIdx == 6);
      DrawMenuValue("Position Weight", FormatFloat(g_ShakePosWeight, 2),
                    lineWidth, 9.0, 60.0 + 7 * 36.0, 0.0, 9.0, activeIdx == 7);
      DrawMenuValue("Stop When Still", FormatBool(g_ShakeStopWhenStill),
                    lineWidth, 9.0, 60.0 + 8 * 36.0, 0.0, 9.0, activeIdx == 8);
      DrawMenuValue("Randomize Pattern", "Press Enter", lineWidth, 9.0,
                    60.0 + 9 * 36.0, 0.0, 9.0, activeIdx == 9);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == 0) {
        g_ShakeEnabled = !g_ShakeEnabled;
        SetStatusText(g_ShakeEnabled ? "Camera shake enabled"
                                     : "Camera shake disabled");
        waitTime = 200;
      } else if (activeIdx == 1) {
        // Enter cycles preset forward through the 5 real presets
        int next = (g_ShakePreset + 1) % 5;
        ApplyShakePreset(next);
        SetStatusText(std::string("Preset: ") + kShakePresetNames[next]);
        waitTime = 200;
      } else {
        // Numeric rows: open on-screen keyboard for precise entry
        float newVal = 0.0f;
        bool ok = false;
        if (activeIdx == 2) {
          if (PromptForFloat(newVal, g_ShakeAmp)) { g_ShakeAmp = newVal; ok = true; }
        } else if (activeIdx == 3) {
          if (PromptForFloat(newVal, g_ShakeFreq)) { g_ShakeFreq = newVal; ok = true; }
        } else if (activeIdx == 4) {
          if (PromptForFloat(newVal, g_ShakeSpeedAmpCoupling)) { g_ShakeSpeedAmpCoupling = newVal; ok = true; }
        } else if (activeIdx == 5) {
          if (PromptForFloat(newVal, g_ShakeSpeedFreqCoupling)) { g_ShakeSpeedFreqCoupling = newVal; ok = true; }
        } else if (activeIdx == 6) {
          if (PromptForFloat(newVal, g_ShakeRotWeight)) { g_ShakeRotWeight = newVal; ok = true; }
        } else if (activeIdx == 7) {
          if (PromptForFloat(newVal, g_ShakePosWeight)) { g_ShakePosWeight = newVal; ok = true; }
        } else if (activeIdx == 8) {
          g_ShakeStopWhenStill = !g_ShakeStopWhenStill;
          SetStatusText(g_ShakeStopWhenStill
                            ? "Shake stops when camera is still"
                            : "Shake always active");
        } else if (activeIdx == 9) {
          RandomizeShakePattern();
          SetStatusText("Shake pattern randomized");
        }
        if (ok) {
          ClampShake();
          MarkShakeCustom();
        }
        waitTime = 200;
      }
    }

    // Numeric rows (2-7, plus the variable-step Freq row) accelerate on hold;
    // the Preset row (1) is an enum cycle and keeps a fixed repeat.
    bool numericRow = activeIdx >= 2 && activeIdx <= 7;
    DWORD adjWait = 100;
    float mult = HoldAccel((bRight || bLeft) && numericRow, holdStart,
                           wasHolding, &adjWait);

    if (bRight) {
      MenuBeep();
      if (activeIdx == 1) {
        int next = (g_ShakePreset + 1) % 5;
        ApplyShakePreset(next);
      } else if (activeIdx == 2) {
        g_ShakeAmp += 0.05f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 3) {
        // Variable step: fine control at low Hz, coarser when already high
        float step = (g_ShakeFreq < 0.5f) ? 0.05f
                   : (g_ShakeFreq < 2.0f) ? 0.1f
                                          : 0.5f;
        g_ShakeFreq += step * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 4) {
        g_ShakeSpeedAmpCoupling += 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 5) {
        g_ShakeSpeedFreqCoupling += 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 6) {
        g_ShakeRotWeight += 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 7) {
        g_ShakePosWeight += 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      }
      waitTime = adjWait;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 1) {
        int prev = (g_ShakePreset <= 0) ? 4 : g_ShakePreset - 1;
        if (prev > 4) prev = 4;
        ApplyShakePreset(prev);
      } else if (activeIdx == 2) {
        g_ShakeAmp -= 0.05f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 3) {
        // Mirror the right-arrow's adaptive step so increments and
        // decrements feel symmetrical at any current value.
        float step = (g_ShakeFreq <= 0.5f) ? 0.05f
                   : (g_ShakeFreq <= 2.0f) ? 0.1f
                                           : 0.5f;
        g_ShakeFreq -= step * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 4) {
        g_ShakeSpeedAmpCoupling -= 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 5) {
        g_ShakeSpeedFreqCoupling -= 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 6) {
        g_ShakeRotWeight -= 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 7) {
        g_ShakePosWeight -= 0.1f * mult;
        ClampShake();
        MarkShakeCustom();
      }
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Auto Drive submenu
//  AI drives the player's current land vehicle (to the map waypoint
//  or wandering) while the free camera flies independently.
// ============================================================

static void ProcessAutoDriveMenu() {
  const float lineWidth = 320.0f;
  const int lineCount = 4;
  int activeIdx = 0;

  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      DrawMenuLine("AUTO DRIVE", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);

      DrawMenuValue("Enabled", FormatBool(g_AutoDriveEnabled), lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Destination",
                    g_AutoDriveMode == 0 ? "Go To Waypoint" : "Drive Anywhere",
                    lineWidth, 9.0, 60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      // Speed stored in m/s; show km/h (1 m/s = 3.6 km/h).
      DrawMenuValue("Speed", FormatInt((int)(g_AutoDriveSpeed * 3.6f + 0.5f)) +
                                 " km/h",
                    lineWidth, 9.0, 60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Driving Style", g_AutoDriveStyleNames[g_AutoDriveStyleIndex],
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);

      // Hint when waypoint mode is on but the player hasn't set a pin.
      if (g_AutoDriveEnabled && g_AutoDriveMode == 0 &&
          UI::IS_WAYPOINT_ACTIVE() == 0) {
        UI::SET_TEXT_FONT(0);
        UI::SET_TEXT_SCALE(0.0f, 0.30f);
        UI::SET_TEXT_COLOUR(255, 200, 80, 255);
        UI::SET_TEXT_CENTRE(1);
        UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
        UI::SET_TEXT_EDGE(1, 0, 0, 0, 205);
        UI::_SET_TEXT_ENTRY((char *)"STRING");
        UI::_ADD_TEXT_COMPONENT_STRING(
            (LPSTR) "Set a waypoint on the map to start driving");
        UI::_DRAW_TEXT(0.5f, 0.62f);
      }

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) {
      MenuBeep();
      break;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == 0) {
        if (g_AutoDriveEnabled) {
          AutoDrive_Stop(); // clears the task + restores freeze state
          SetStatusText("Auto Drive off");
        } else {
          // Only meaningful with a vehicle, but enable regardless — the tick
          // idles until the player is in one.
          g_AutoDriveEnabled = true;
          SetStatusText(g_AutoDriveMode == 0
                            ? "Auto Drive on — set a waypoint"
                            : "Auto Drive on — wandering");
        }
      } else if (activeIdx == 1) {
        g_AutoDriveMode = (g_AutoDriveMode == 0) ? 1 : 0;
        SetStatusText(g_AutoDriveMode == 0 ? "Driving to waypoint"
                                           : "Driving anywhere");
      }
      waitTime = 200;
    }

    DWORD adjWait = 100;
    float mult = HoldAccel(bRight || bLeft, holdStart, wasHolding, &adjWait);

    if (bLeft || bRight) {
      MenuBeep();
      int dir = bRight ? 1 : -1;
      switch (activeIdx) {
      case 1:
        g_AutoDriveMode = (g_AutoDriveMode == 0) ? 1 : 0;
        break;
      case 2: {
        // Adjust in ~5 km/h steps (converted to m/s), 5..360 km/h.
        g_AutoDriveSpeed += dir * (5.0f / 3.6f) * mult;
        if (g_AutoDriveSpeed < 5.0f / 3.6f)
          g_AutoDriveSpeed = 5.0f / 3.6f;
        if (g_AutoDriveSpeed > 100.0f)
          g_AutoDriveSpeed = 100.0f;
        break;
      }
      case 3:
        g_AutoDriveStyleIndex += dir;
        if (g_AutoDriveStyleIndex < 0)
          g_AutoDriveStyleIndex = g_AutoDriveStyleCount - 1;
        if (g_AutoDriveStyleIndex >= g_AutoDriveStyleCount)
          g_AutoDriveStyleIndex = 0;
        break;
      }
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Free Camera Mode — main menu
//  Returns true if user chose "Exit" (caller re-opens picker);
//  false on normal close (bBack / F5 toggle).
// ============================================================

static bool ProcessFreeCameraMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 7;
  int activeIdx = 0;

  // Entering Free Camera mode *is* enabling the flycam — there's no manual
  // toggle row anymore. Guard against any desync (e.g. menu reopened while
  // somehow inactive) by ensuring the camera is live before we draw.
  if (!g_FreeCamActive) {
    InitFreeCamera();
    SetStatusText("Simple Camera ON");
  }

  static const char *lineCaption[lineCount] = {
      "Movement",       "Lens settings", "Depth of field", "Camera effects",
      "World & scene",  "Misc settings", "Exit"};

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();

      // Title bar
      std::string title = "SIMPLE CAMERA";
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Menu items — all plain submenu rows now that the toggle is gone.
      for (int i = 0; i < lineCount; i++) {
        std::string cap = lineCaption[i];
        if (i != activeIdx)
          DrawMenuLine(cap, "", lineWidth, 9.0, 60.0 + i * 36.0, 0.0, 9.0,
                       false, false);
        else
          DrawMenuLine(cap, "", lineWidth + 1.0, 11.0, 56.0 + i * 36.0, 0.0,
                       7.0, true, false);
      }

      // Keep camera/features alive while browsing the menu
      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    // Process buttons
    bool bSelect, bBack, bUp, bDown;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, NULL, NULL);

    if (bSelect) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        ProcessMovementMenu();
        break;
      case 1:
        ProcessLensMenu();
        break;
      case 2:
        ProcessDoFMenu();
        break;
      case 3:
        ProcessCameraEffectsMenu();
        break;
      case 4:
        ProcessWorldSceneMenu();
        break;
      case 5:
        ProcessMiscMenu();
        break;
      case 6:
        // Exit → caller pops back to picker
        return true;
      }
      waitTime = 200;
    } else if (bBack || IsMenuTogglePressed()) {
      MenuBeep();
      return false;
    } else if (bUp) {
      MenuBeep();
      if (activeIdx == 0)
        activeIdx = lineCount;
      activeIdx--;
      waitTime = 150;
    } else if (bDown) {
      MenuBeep();
      activeIdx++;
      if (activeIdx == lineCount)
        activeIdx = 0;
      waitTime = 150;
    }
  }
}

// ============================================================
//  Mode Picker — first F5 in a session, or whenever the user
//  explicitly switches modes via Exit.
//  Returns 0 = Free Camera, 1 = Camera Sequence, -1 = cancelled.
// ============================================================

static int ProcessModePickerMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 2;
  int activeIdx = 0;
  if (g_CameraMode >= 0 && g_CameraMode <= 1) activeIdx = g_CameraMode;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();

      DrawMenuLine("SIMPLE CAMERA", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Free Camera", "Manual flight", lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Camera Sequence", "Keyframed animation", lineWidth, 9.0,
                    60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, NULL, NULL);

    if (bSelect) {
      MenuBeep();
      return activeIdx;
    }
    if (bBack || IsMenuTogglePressed()) {
      MenuBeep();
      return -1;
    }
    if (bUp) {
      MenuBeep();
      activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1;
      waitTime = 150;
    }
    if (bDown) {
      MenuBeep();
      activeIdx = (activeIdx + 1) % lineCount;
      waitTime = 150;
    }
  }
}

// ============================================================
//  Camera Sequence Mode — main menu + list submenus + per-item editors
//  Returns true if user chose "Exit", false on normal close.
// ============================================================

// Forward decl — full implementation appended below.
static bool ProcessSequenceMenu();
static void ProcessPoseListMenu();
static void ProcessPoseEditMenu(int poseIdx);
static void ProcessEventListMenu();
static void ProcessEventEditMenu(int eventIdx);
static void ProcessSequenceFollowMenu();

// Step size for left/right adjustment of an event's value, picked by
// kind: integer-valued kinds (toggles + preset index + trigger) step
// by 1, continuous floats by 0.1.
static float EventValueStep(EffectKind k) {
  switch (k) {
  case EFX_SHAKE_ENABLED:
  case EFX_SHAKE_PRESET:
  case EFX_SHAKE_STOP_STILL:
  case EFX_SHAKE_RANDOMIZE:
    return 1.0f;
  default:
    return 0.1f;
  }
}

// ============================================================
//  Top-level dispatch (called by script.cpp on F5)
// ============================================================

void ProcessConfigMenu() {
  // Flag the menu as open for the whole time we're in here (covers every
  // submenu). The guard clears it on every return path. UpdateFreeCamera uses
  // it to stop the controller D-Pad from zooming while it navigates the menu.
  g_MenuOpen = true;
  struct MenuOpenGuard { ~MenuOpenGuard() { g_MenuOpen = false; } } _mog;

  while (true) {
    // First-time-ever F5: show picker. After that, F5 reopens the
    // remembered mode menu directly. Switching modes pops back here.
    if (g_CameraMode == -1) {
      int chosen = ProcessModePickerMenu();
      if (chosen == -1) return;          // user cancelled
      // Auto-stop whichever mode might have been running (safety).
      if (g_FreeCamActive && chosen != 0) DestroyFreeCamera();
      if (Sequence_IsInMode() && chosen != 1) Sequence_ExitMode();
      g_CameraMode = chosen;
      if (g_CameraMode == 1) {
        Sequence_EnterMode();
      } else if (g_CameraMode == 0 && !g_FreeCamActive) {
        // Picking Free Camera engages the flycam immediately — skips the
        // old "pick mode -> open menu -> toggle freecam" three-step dance.
        InitFreeCamera();
        SetStatusText("Simple Camera ON");
      }
    }

    bool wantSwitch = false;
    if (g_CameraMode == 0) {
      wantSwitch = ProcessFreeCameraMenu();
    } else if (g_CameraMode == 1) {
      wantSwitch = ProcessSequenceMenu();
    }

    if (!wantSwitch) return;             // normal close (bBack / F5 toggle)

    // User picked Exit → tear down current mode, return to picker.
    if (g_CameraMode == 0 && g_FreeCamActive) DestroyFreeCamera();
    if (g_CameraMode == 1) Sequence_ExitMode();
    g_CameraMode = -1;
  }
}

// ============================================================
//  Sequence → Render to Image Sequence (Phase 2)
// ============================================================
//
// Deterministic offline render: step the playhead by exactly 1/fps, let the
// scene settle (TAA / streaming), then ask the ReShade addon to grab the
// frame — taking as long as each frame needs, independent of real FPS. World
// entities are pinned so only the camera moves; the result is a numbered PNG
// sequence ready for ffmpeg. (Camera-led shots; animations still creep under
// entity-freeze, per the engine limits we established.)

static float g_RenderFps = 60.0f;     // output frame rate
static int g_RenderSettleFrames = 8;  // frames to wait per step before grabbing
static int g_RenderFlushFrames = 3;   // clean frames after the banner, before grabbing
static int g_RenderBlurSamples = 1;   // motion-blur sub-samples per frame (1 = off)
static float g_RenderShutter = 0.5f;  // shutter window as a fraction of the frame interval
static int g_RenderBlurStrength = 100; // % multiplier on the blur length (shutter window)
static int g_RenderFormat = 0;        // 0 = PNG (lossless), 1 = JPEG (lightweight)
static int g_RenderJpegQuality = 90;  // JPEG quality 1..100
static float g_RenderSlowmo = 0.0f;   // capture time scale. 0 = AUTO: pick a safe
                                      // value so each frame's capture work fits
                                      // the 1/fps budget (synced stays in sync).
                                      // >0 = fixed time scale.
static int g_RenderSyncWorld = 0;     // 0 = Camera-led (static world, scrub +
                                      // accumulation blur). 1 = Synced (play the
                                      // world + camera on one clock so world
                                      // motion matches camera speed exactly).
static float g_RenderHighlightBoost = 0.3f; // 0..0.99 — highlight lift in blur accumulation

// Centered progress banner. Only drawn on settle frames — never on the frame
// the addon actually captures, so it can't bleed into the output.
static void DrawRenderProgress(int done, int total, const char *folder) {
  char buf[160];
  sprintf_s(buf, "RENDERING  %d / %d   (Backspace = cancel)", done, total);
  UI::SET_TEXT_FONT(0);
  UI::SET_TEXT_SCALE(0.6f, 0.6f);
  UI::SET_TEXT_COLOUR(255, 255, 255, 255);
  UI::SET_TEXT_WRAP(0.0, 1.0);
  UI::SET_TEXT_CENTRE(1);
  UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
  UI::SET_TEXT_EDGE(1, 0, 0, 0, 205);
  UI::_SET_TEXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)buf);
  UI::_DRAW_TEXT(0.5f, 0.45f);
}

static void ProcessRenderToImages() {
  CameraSequence *s = Sequence_Active();
  if (!s || (int)s->poses.size() < 2) {
    SetStatusText("Need at least 2 keyframes to render");
    return;
  }
  if (!FxCapture_AddonPresent()) {
    SetStatusText("Image rendering needs ReShade + IgcsConnector addon. "
                  "Install both (see README), then restart.");
    return;
  }

  float dur = Sequence_TotalDuration();
  // Honor the sequence "Speed": it scales how fast the shot plays, so a render
  // advances the timeline by `speed/fps` per output frame and produces
  // `duration/speed * fps` frames. Speed > 1 = faster/shorter; < 1 = slower/
  // longer. Works in both modes (Synced stays in sync because camera + world
  // both advance by the same per-frame sequence step).
  float renderSpeed = (s->playbackSpeed > 0.0001f) ? s->playbackSpeed : 1.0f;
  const float stepT = renderSpeed / g_RenderFps; // sequence-time per output frame
  int total = (int)(dur / renderSpeed * g_RenderFps + 0.5f);
  if (total < 1) {
    SetStatusText("Sequence too short to render");
    return;
  }

  char folder[MAX_PATH];
  if (!FxCapture_NewSequenceFolder(folder, sizeof(folder))) {
    SetStatusText("Couldn't create output folder");
    return;
  }

  int samples = g_RenderBlurSamples;
  if (samples < 1) samples = 1;

  // Save state we override during the render, restore on exit.
  bool prevFreezeEnt = g_FreezeEntities;
  bool prevHideHUD = g_HideHUD;
  bool prevOverlay = g_ShowInfoOverlay;
  bool prevMarkers = g_SequenceShowMarkers;
  float prevTime = Sequence_CurrentTime();
  if (Sequence_IsPlaying()) Sequence_Stop();

  // Hide the keyframe markers / path preview — Sequence_FrameTick (used by the
  // synced path) redraws them every frame, so they'd be baked into the output.
  g_SequenceShowMarkers = false;

  // Do NOT freeze entities — that pins positions but leaves wind-driven
  // vegetation, cloth and particles running at full speed (they'd smear in
  // motion blur and flicker between frames). Instead hold a low time scale so
  // ALL physics slow uniformly; the camera is still scrubbed exactly each
  // frame, independent of time scale.
  g_FreezeEntities = false;
  g_HideHUD = true;          // keep the HUD out of the frames
  g_ShowInfoOverlay = false; // and our debug overlay
  SetStatusText("", 0);      // clear any lingering status so it can't be captured

  FxCapture_SetQuality(g_RenderJpegQuality);
  FxCapture_SetHighlightBoost(g_RenderHighlightBoost);
  const char *ext = (g_RenderFormat == 1) ? "jpg" : "png";

  bool cancelled = false;
  bool addonFail = false;
  int progDone = 0;

  if (!g_RenderSyncWorld) {
    // ============================================================
    //  Camera-led (static world) — deterministic scrub + accumulation blur.
    //  World is held near-static (slow-mo); camera scrubbed exactly. Shake +
    //  effect automation are evaluated per output frame from the timeline.
    // ============================================================
    Sequence_RenderEffectsBegin();

    // Camera-led holds the world near-static; Auto just means a low fixed scale.
    const float detSlowmo = (g_RenderSlowmo <= 0.0001f) ? 0.02f : g_RenderSlowmo;

    auto renderTick = [&](float subT, bool drawBanner) {
      g_RenderShakeTime = subT; // time-driven shake for this exact sub-sample
      Sequence_SetCurrentTime(subT);
      DisableMenuPhoneControls();
      UpdateTimeWeather();
      UpdateGlobalEffects();
      GAMEPLAY::SET_TIME_SCALE(detSlowmo);
      if (drawBanner) DrawRenderProgress(progDone, total, folder);
      WAIT(0);
    };

    auto captureSample = [&](float subT, const char *path, int sc, int si) {
      for (int k = 0; k < g_RenderSettleFrames; ++k) {
        renderTick(subT, true);
        bool bBack;
        GetMenuButtons(NULL, &bBack, NULL, NULL, NULL, NULL);
        if (bBack) { cancelled = true; return; }
      }
      for (int k = 0; k < g_RenderFlushFrames; ++k) renderTick(subT, false);
      FxCapture_RequestSample(path, sc, si);
      int guard = 0;
      while (!FxCapture_IsLastDone() && guard < 300) {
        renderTick(subT, false);
        ++guard;
      }
      if (guard >= 300) addonFail = true;
    };

    for (int i = 0; i < total && !cancelled && !addonFail; ++i) {
      progDone = i;
      float t = (float)i * stepT; // sequence time for this output frame
      Sequence_RenderEffectsApply(t);
      char path[MAX_PATH];
      sprintf_s(path, "%s\\frame_%06d.%s", folder, i, ext);

      if (samples <= 1) {
        captureSample(t, path, 1, 0);
      } else {
        float effShutter = g_RenderShutter * (g_RenderBlurStrength / 100.0f);
        for (int j = 0; j < samples && !cancelled && !addonFail; ++j) {
          float subT = t + ((float)j / (float)samples) * effShutter * stepT;
          captureSample(subT, path, samples, j);
        }
      }
    }

    Sequence_RenderEffectsEnd();
  } else {
    // ============================================================
    //  Synced (live world) — playback-driven. The sequence PLAYS at a slow
    //  time scale, so the camera, world, shake and effect events all advance
    //  on the SAME game clock and stay in lockstep. We grab a frame each time
    //  playback crosses the next 1/fps mark → world motion matches camera
    //  speed exactly. Motion blur (when enabled) accumulates M consecutive
    //  live frames per output frame, so it blurs the WHOLE moving scene.
    // ============================================================
    int syncSamples = g_RenderBlurSamples;
    if (syncSamples < 1) syncSamples = 1;

    // Per-output-frame game-time budget = the sequence-time step between output
    // frames (speed-scaled). If the capture work for one output frame consumes
    // MORE than this, playback overshoots the next grid mark and the world runs
    // fast. AUTO tunes the time scale to keep the work inside the budget.
    const float budget = stepT;
    const bool autoSlow = g_RenderSlowmo <= 0.0001f;
    // curSlowmo is the live time scale stepDyn asserts; AUTO adapts it per frame.
    float curSlowmo;
    if (autoSlow) {
      // Initial estimate from fps x work (assumes ~30 real fps; it self-corrects
      // within a few frames from measured consumption anyway).
      int workFrames = 2 + g_RenderFlushFrames + syncSamples;
      curSlowmo = budget / ((float)workFrames * 0.033f) * 0.7f;
      if (curSlowmo < 0.005f) curSlowmo = 0.005f;
      if (curSlowmo > 0.2f) curSlowmo = 0.2f;
    } else {
      curSlowmo = g_RenderSlowmo;
    }

    float savedSpeed = s->playbackSpeed;
    s->playbackSpeed = 1.0f; // 1:1 so output frame i maps to sequence time i/fps
    Sequence_SetCurrentTime(0.0f);
    Sequence_Play(); // Play snapshots/restores shake config itself

    auto stepDyn = [&](bool banner) {
      DisableMenuPhoneControls();
      if (Sequence_IsInMode()) Sequence_FrameTick(); // camera+world+events+shake on one clock
      UpdateTimeWeather();
      UpdateGlobalEffects();
      GAMEPLAY::SET_TIME_SCALE(curSlowmo); // reassert capture slow-mo
      if (banner) DrawRenderProgress(progDone, total, folder);
      WAIT(0);
    };

    int cap = 0;
    while (cap < total && !cancelled && !addonFail) {
      progDone = cap;
      float target = (float)cap * stepT; // sequence time for this output frame
      int safety = 0;
      while (Sequence_CurrentTime() < target && !cancelled) {
        stepDyn(true);
        bool bBack;
        GetMenuButtons(NULL, &bBack, NULL, NULL, NULL, NULL);
        if (bBack) cancelled = true;
        if (!Sequence_IsPlaying()) break; // playback hit the end
        if (++safety > 20000) break;      // hard safety net
      }
      if (cancelled) break;
      // Guarantee a couple of banner frames so progress is always visible —
      // the advance loop above is skipped when the per-frame blur work has
      // already overshot the grid mark.
      for (int k = 0; k < 2 && !cancelled; ++k) {
        stepDyn(true);
        bool bBack;
        GetMenuButtons(NULL, &bBack, NULL, NULL, NULL, NULL);
        if (bBack) cancelled = true;
      }
      if (cancelled) break;
      // Clean frames so the banner clears the pipeline before grabbing.
      for (int k = 0; k < g_RenderFlushFrames; ++k) stepDyn(false);

      char path[MAX_PATH];
      sprintf_s(path, "%s\\frame_%06d.%s", folder, cap, ext);

      // Measure how much game-time the capture work consumes so AUTO can adapt.
      float tWorkStart = Sequence_CurrentTime();

      // Accumulate `syncSamples` consecutive live frames. The world advances a
      // little between each (slowed playback), so the addon's linear average
      // becomes true full-scene motion blur. 1 = no blur.
      for (int j = 0; j < syncSamples && !addonFail; ++j) {
        FxCapture_RequestSample(path, syncSamples, j);
        int guard = 0;
        while (!FxCapture_IsLastDone() && guard < 300) { stepDyn(false); ++guard; }
        if (guard >= 300) addonFail = true;
      }

      // AUTO: nudge the time scale so the per-frame work lands inside the
      // budget. Too much consumed -> slow down (world was overshooting); plenty
      // of headroom -> speed up (don't waste wall-clock). Self-correcting.
      if (autoSlow) {
        float consumed = Sequence_CurrentTime() - tWorkStart;
        if (consumed > budget * 0.85f) curSlowmo *= 0.7f;
        else if (consumed < budget * 0.40f) curSlowmo *= 1.15f;
        if (curSlowmo < 0.003f) curSlowmo = 0.003f;
        if (curSlowmo > 0.5f) curSlowmo = 0.5f;
      }
      ++cap;
    }

    Sequence_Stop();
    s->playbackSpeed = savedSpeed;
  }

  // Restore.
  g_RenderShakeTime = -1.0f; // back to live dt-driven shake
  GAMEPLAY::SET_TIME_SCALE(1.0f);
  g_FreezeEntities = prevFreezeEnt;
  g_HideHUD = prevHideHUD;
  g_ShowInfoOverlay = prevOverlay;
  g_SequenceShowMarkers = prevMarkers;
  Sequence_SetCurrentTime(prevTime);

  if (addonFail) {
    SetStatusText("Capture addon not responding — aborted");
  } else if (cancelled) {
    SetStatusText("Render cancelled");
  } else {
    char msg[96];
    sprintf_s(msg, "Render complete: %d frames", total);
    SetStatusText(msg);
  }
}

// Common video frame rates for the Output FPS cycler.
static const float kRenderFpsPresets[] = {24.0f, 25.0f,  30.0f,  48.0f,
                                          50.0f, 60.0f, 120.0f, 240.0f};
static const int kRenderFpsPresetCount =
    (int)(sizeof(kRenderFpsPresets) / sizeof(kRenderFpsPresets[0]));

static void CycleRenderFps(int dir) {
  // Snap to the nearest preset, then step in `dir`.
  int idx = 0;
  float best = 1e9f;
  for (int i = 0; i < kRenderFpsPresetCount; ++i) {
    float d = fabsf(kRenderFpsPresets[i] - g_RenderFps);
    if (d < best) { best = d; idx = i; }
  }
  idx += dir;
  if (idx < 0) idx = 0;
  if (idx >= kRenderFpsPresetCount) idx = kRenderFpsPresetCount - 1;
  g_RenderFps = kRenderFpsPresets[idx];
}

// Motion-blur sample-count presets (1 = off).
static const int kBlurPresets[] = {1, 2, 4, 8, 16, 32, 64};
static const int kBlurPresetCount =
    (int)(sizeof(kBlurPresets) / sizeof(kBlurPresets[0]));

static void CycleBlurSamples(int dir) {
  int idx = 0, best = 1 << 30;
  for (int i = 0; i < kBlurPresetCount; ++i) {
    int d = kBlurPresets[i] - g_RenderBlurSamples;
    if (d < 0) d = -d;
    if (d < best) { best = d; idx = i; }
  }
  idx += dir;
  if (idx < 0) idx = 0;
  if (idx >= kBlurPresetCount) idx = kBlurPresetCount - 1;
  g_RenderBlurSamples = kBlurPresets[idx];
}

// Render settings submenu.
static void ProcessRenderMenu() {
  const float lineWidth = 340.0f;
  const int lineCount = 12;
  int activeIdx = 0;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      const bool synced = g_RenderSyncWorld != 0;
      const bool samplesOn = g_RenderBlurSamples > 1;
      const bool shutterApplies = samplesOn && !synced; // shutter/strength: camera-led only
      DisableMenuPhoneControls();
      DrawMenuLine("RENDER TO IMAGES", "", lineWidth, 15.0, 18.0, 0.0, 5.0,
                   false, true);

      DrawMenuValue("Render Mode",
                    synced ? "Synced (live world)" : "Camera-led (static)",
                    lineWidth, 9.0, 60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);

      char fpsVal[48];
      int frames = (int)(Sequence_TotalDuration() * g_RenderFps + 0.5f);
      sprintf_s(fpsVal, "%d fps  -  %d frames", (int)g_RenderFps, frames);
      DrawMenuValue("Output FPS", fpsVal, lineWidth, 9.0, 60.0 + 1 * 36.0, 0.0,
                    9.0, activeIdx == 1);

      DrawMenuValue("Settle Frames",
                    synced ? std::string("n/a") : FormatInt(g_RenderSettleFrames),
                    lineWidth, 9.0, 60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Flush Frames", FormatInt(g_RenderFlushFrames), lineWidth,
                    9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);

      // Motion blur works in BOTH modes now (synced accumulates live frames).
      char blurVal[48];
      if (!samplesOn) sprintf_s(blurVal, "Off");
      else sprintf_s(blurVal, "%d samples", g_RenderBlurSamples);
      DrawMenuValue("Motion Blur", blurVal, lineWidth, 9.0, 60.0 + 4 * 36.0, 0.0,
                    9.0, activeIdx == 4);

      // Shutter / Strength only meaningful for camera-led accumulation (synced
      // blur spans consecutive live frames instead of a scrub window).
      char shutterVal[32];
      if (!shutterApplies) sprintf_s(shutterVal, "n/a");
      else sprintf_s(shutterVal, "%d deg", (int)(g_RenderShutter * 360.0f + 0.5f));
      DrawMenuValue("Shutter", shutterVal, lineWidth, 9.0, 60.0 + 5 * 36.0, 0.0,
                    9.0, activeIdx == 5);

      char strVal[24];
      if (!shutterApplies) sprintf_s(strVal, "n/a");
      else sprintf_s(strVal, "%d%%", g_RenderBlurStrength);
      DrawMenuValue("Blur Strength", strVal, lineWidth, 9.0, 60.0 + 6 * 36.0,
                    0.0, 9.0, activeIdx == 6);

      // Highlight Boost — extra highlight lift in the linear blur accumulation
      // (brighter streaks). Applies to blur in either mode.
      char hbVal[24];
      if (!samplesOn) sprintf_s(hbVal, "n/a");
      else sprintf_s(hbVal, "%d%%", (int)(g_RenderHighlightBoost * 100.0f + 0.5f));
      DrawMenuValue("Highlight Boost", hbVal, lineWidth, 9.0, 60.0 + 7 * 36.0,
                    0.0, 9.0, activeIdx == 7);

      DrawMenuValue("Format", g_RenderFormat == 1 ? "JPEG" : "PNG (lossless)",
                    lineWidth, 9.0, 60.0 + 8 * 36.0, 0.0, 9.0, activeIdx == 8);
      char qVal[24];
      if (g_RenderFormat == 1) sprintf_s(qVal, "%d", g_RenderJpegQuality);
      else sprintf_s(qVal, "n/a");
      DrawMenuValue("JPEG Quality", qVal, lineWidth, 9.0, 60.0 + 9 * 36.0, 0.0,
                    9.0, activeIdx == 9);

      char slowmoVal[24];
      if (g_RenderSlowmo <= 0.0001f) sprintf_s(slowmoVal, "Auto");
      else sprintf_s(slowmoVal, "%d%%", (int)(g_RenderSlowmo * 100.0f + 0.5f));
      DrawMenuValue("World Slow-mo", slowmoVal, lineWidth, 9.0, 60.0 + 10 * 36.0,
                    0.0, 9.0, activeIdx == 10);

      bool addonReady = FxCapture_AddonPresent();
      DrawMenuValue("Start Render",
                    addonReady ? "Press Enter" : "Needs ReShade + IGCS addon",
                    lineWidth, 9.0, 60.0 + 11 * 36.0, 0.0, 9.0, activeIdx == 11);
      // Persistent notice when the capture addon isn't loaded, so it's obvious
      // why rendering won't work without drilling into Start.
      if (!addonReady) {
        UI::SET_TEXT_FONT(0);
        UI::SET_TEXT_SCALE(0.0f, 0.30f);
        UI::SET_TEXT_COLOUR(255, 200, 80, 255);
        UI::SET_TEXT_CENTRE(1);
        UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
        UI::SET_TEXT_EDGE(1, 0, 0, 0, 205);
        UI::_SET_TEXT_ENTRY((char *)"STRING");
        UI::_ADD_TEXT_COMPONENT_STRING(
            (LPSTR) "Rendering requires ReShade (add-on support) + "
                    "IgcsConnector addon");
        UI::_DRAW_TEXT(0.5f, 0.62f);
      }

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) { MenuBeep(); break; }
    if (bUp) { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == 0) g_RenderSyncWorld = g_RenderSyncWorld ? 0 : 1;
      else if (activeIdx == 8) g_RenderFormat = (g_RenderFormat == 1) ? 0 : 1;
      else if (activeIdx == 11) ProcessRenderToImages();
      waitTime = 200;
    }
    if (bRight) {
      MenuBeep();
      if (activeIdx == 0) g_RenderSyncWorld = g_RenderSyncWorld ? 0 : 1;
      else if (activeIdx == 1) CycleRenderFps(+1);
      else if (activeIdx == 2) { if (++g_RenderSettleFrames > 60) g_RenderSettleFrames = 60; }
      else if (activeIdx == 3) { if (++g_RenderFlushFrames > 20) g_RenderFlushFrames = 20; }
      else if (activeIdx == 4) CycleBlurSamples(+1);
      else if (activeIdx == 5) { g_RenderShutter += 0.05f; if (g_RenderShutter > 1.0f) g_RenderShutter = 1.0f; }
      else if (activeIdx == 6) { g_RenderBlurStrength += 10; if (g_RenderBlurStrength > 200) g_RenderBlurStrength = 200; }
      else if (activeIdx == 7) { g_RenderHighlightBoost += 0.05f; if (g_RenderHighlightBoost > 0.95f) g_RenderHighlightBoost = 0.95f; }
      else if (activeIdx == 8) g_RenderFormat = (g_RenderFormat == 1) ? 0 : 1;
      else if (activeIdx == 9) { g_RenderJpegQuality += 5; if (g_RenderJpegQuality > 100) g_RenderJpegQuality = 100; }
      else if (activeIdx == 10) { if (g_RenderSlowmo < 0.0001f) g_RenderSlowmo = 0.01f; else { g_RenderSlowmo += 0.01f; if (g_RenderSlowmo > 1.0f) g_RenderSlowmo = 1.0f; } }
      waitTime = 120;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 0) g_RenderSyncWorld = g_RenderSyncWorld ? 0 : 1;
      else if (activeIdx == 1) CycleRenderFps(-1);
      else if (activeIdx == 2) { if (--g_RenderSettleFrames < 0) g_RenderSettleFrames = 0; }
      else if (activeIdx == 3) { if (--g_RenderFlushFrames < 0) g_RenderFlushFrames = 0; }
      else if (activeIdx == 4) CycleBlurSamples(-1);
      else if (activeIdx == 5) { g_RenderShutter -= 0.05f; if (g_RenderShutter < 0.05f) g_RenderShutter = 0.05f; }
      else if (activeIdx == 6) { g_RenderBlurStrength -= 10; if (g_RenderBlurStrength < 10) g_RenderBlurStrength = 10; }
      else if (activeIdx == 7) { g_RenderHighlightBoost -= 0.05f; if (g_RenderHighlightBoost < 0.0f) g_RenderHighlightBoost = 0.0f; }
      else if (activeIdx == 8) g_RenderFormat = (g_RenderFormat == 1) ? 0 : 1;
      else if (activeIdx == 9) { g_RenderJpegQuality -= 5; if (g_RenderJpegQuality < 10) g_RenderJpegQuality = 10; }
      else if (activeIdx == 10) { g_RenderSlowmo -= 0.01f; if (g_RenderSlowmo < 0.005f) g_RenderSlowmo = 0.0f; } // 0 = Auto
      waitTime = 120;
    }
  }
}

// ============================================================
//  Camera Sequence — main menu
// ============================================================

static std::string FormatTimeSec(float t) {
  char buf[16];
  sprintf_s(buf, "%.2fs", t);
  return std::string(buf);
}

static bool ProcessSequenceMenu() {
  const float lineWidth = 320.0f;
  // 18 rows. "World & Scene..." and "Render to Images..." ride just above Exit.
  //
  // The row count exceeds kMaxVisibleListRows (12), so we render through
  // the same scroll-window pattern that ProcessPoseListMenu / Movement
  // Settings use: scrollOffset persists across the inner draw loop,
  // EnsureRowVisible keeps the cursor in view, vis() / rowTop() drive
  // per-row visibility and Y placement.
  const int lineCount = 18;
  int activeIdx = 0;
  int scrollOffset = 0;

  DWORD holdStart = 0;
  bool wasHolding = false;
  // "Delete Active Sequence" arms on first Enter, commits on a second press
  // within a few seconds — guards against accidentally wiping a sequence.
  DWORD deleteArmedUntil = 0;

  DWORD waitTime = 150;
  while (true) {
    int seqIdx = Sequence_ActiveIndex();
    int seqCount = Sequence_Count();
    EnsureRowVisible(activeIdx, lineCount, scrollOffset);
    int visEnd = scrollOffset + kMaxVisibleListRows;
    if (visEnd > lineCount) visEnd = lineCount;

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();

      CameraSequence *s = Sequence_Active();
      std::string seqLabel = s ? s->name : "(none)";
      char seqIdxBuf[32]; sprintf_s(seqIdxBuf, " [%d/%d]", seqIdx + 1, seqCount);
      seqLabel += seqIdxBuf;

      // Title bar shows scroll range when not all rows fit on screen, so
      // the user can tell at a glance there's more content above / below.
      char title[64];
      if (lineCount > kMaxVisibleListRows) {
        sprintf_s(title, "CAMERA SEQUENCE  [%d-%d / %d]", scrollOffset + 1,
                  visEnd, lineCount);
      } else {
        sprintf_s(title, "CAMERA SEQUENCE");
      }
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Visibility predicate + Y mapping for the scroll viewport. Each
      // logical row `r` only renders if it falls inside [scrollOffset,
      // scrollOffset + kMaxVisibleListRows); `rowTop(r)` maps it to the
      // on-screen Y by subtracting the scroll offset.
      auto vis = [&](int r) {
        return r >= scrollOffset && r < scrollOffset + kMaxVisibleListRows;
      };
      auto rowTop = [&](int r) { return 60.0f + (r - scrollOffset) * 36.0f; };

      if (vis(0))
        DrawMenuValue("Sequence", seqLabel, lineWidth, 9.0,
                      rowTop(0), 0.0, 9.0, activeIdx == 0);
      if (vis(1))
        DrawMenuValue("Play / Pause",
                      Sequence_IsPlaying() ? "Playing" : "Paused", lineWidth,
                      9.0, rowTop(1), 0.0, 9.0, activeIdx == 1);
      if (vis(2))
        DrawMenuValue("Stop", "Press Enter", lineWidth, 9.0, rowTop(2), 0.0,
                      9.0, activeIdx == 2);
      // Loop value shows closed-status when on, so the user can tell
      // whether the wrap is seamless or jumpy without checking the gap.
      std::string loopStr;
      if (!s || !s->loop) {
        loopStr = "Off";
      } else if (Sequence_IsLoopClosed()) {
        loopStr = "On (closed)";
      } else {
        loopStr = "On (open)";
      }
      if (vis(3))
        DrawMenuValue("Loop", loopStr, lineWidth, 9.0,
                      rowTop(3), 0.0, 9.0, activeIdx == 3);
      if (vis(4))
        DrawMenuValue("Speed", FormatFloat(s ? s->playbackSpeed : 1.0f, 2),
                      lineWidth, 9.0, rowTop(4), 0.0, 9.0, activeIdx == 4);

      char timeStr[48];
      sprintf_s(timeStr, "%.2fs / %.2fs", Sequence_CurrentTime(),
                Sequence_TotalDuration());
      if (vis(5))
        DrawMenuValue("Time", timeStr, lineWidth, 9.0, rowTop(5), 0.0, 9.0,
                      activeIdx == 5);

      char countBuf[32];
      if (vis(6)) {
        sprintf_s(countBuf, "%d", s ? (int)s->poses.size() : 0);
        DrawMenuValue("Pose Keyframes...", countBuf, lineWidth, 9.0,
                      rowTop(6), 0.0, 9.0, activeIdx == 6);
      }
      if (vis(7)) {
        sprintf_s(countBuf, "%d", s ? (int)s->events.size() : 0);
        DrawMenuValue("Effect Events...", countBuf, lineWidth, 9.0,
                      rowTop(7), 0.0, 9.0, activeIdx == 7);
      }

      if (vis(8))
        DrawMenuValue("Capture Current Pose", "F6", lineWidth, 9.0,
                      rowTop(8), 0.0, 9.0, activeIdx == 8);
      if (vis(9))
        DrawMenuValue("New Sequence", "Press Enter", lineWidth, 9.0,
                      rowTop(9), 0.0, 9.0, activeIdx == 9);
      if (vis(10))
        DrawMenuValue("Delete Active Sequence",
                      (GetTickCount() < deleteArmedUntil) ? "Press again to confirm"
                                                          : "Press Enter",
                      lineWidth, 9.0, rowTop(10), 0.0, 9.0, activeIdx == 10);
      if (vis(11))
        DrawMenuValue("Save All to INI", "Press Enter", lineWidth, 9.0,
                      rowTop(11), 0.0, 9.0, activeIdx == 11);
      // Close Loop — shows gap diagnostics when there's something to
      // close, so the user can see how far off the seam is before acting.
      // Uses ASCII only ("d=" / "deg") because the GTA Chalet Comprime
      // font lacks glyphs for Δ and ° at small sizes — they render as
      // missing-glyph boxes.
      if (vis(12)) {
        std::string closeStr = "Press Enter";
        LoopGap gap;
        if (Sequence_GetLoopGap(&gap)) {
          char buf[64];
          sprintf_s(buf, "d=%.2fm / %.1f deg", gap.posDist,
                    gap.pitchDelta + gap.yawDelta + gap.rollDelta);
          closeStr = buf;
        }
        DrawMenuValue("Close Loop", closeStr, lineWidth, 9.0,
                      rowTop(12), 0.0, 9.0, activeIdx == 12);
      }

      // "Follow & Entity Lock..." — value column shows a quick status
      // glance: current follow target + how many keyframes in the active
      // sequence carry a lock. Saves the user a drill-down to check.
      if (vis(13)) {
        const char *followLabel = "None";
        if (g_FollowMode == 1)      followLabel = "Player";
        else if (g_FollowMode == 2) followLabel = (g_FollowTargetEntity != 0)
                                                    ? "Aimed (locked)"
                                                    : "Aimed (unlocked)";
        char followVal[64];
        int lockedCount = Sequence_LockedPoseCount();
        int totalPoses  = s ? (int)s->poses.size() : 0;
        sprintf_s(followVal, "%s  -  %d/%d locked", followLabel, lockedCount,
                  totalPoses);
        DrawMenuValue("Follow & Entity Lock...", followVal, lineWidth, 9.0,
                      rowTop(13), 0.0, 9.0, activeIdx == 13);
      }

      if (vis(14))
        DrawMenuValue("Show Markers", FormatBool(g_SequenceShowMarkers),
                      lineWidth, 9.0, rowTop(14), 0.0, 9.0,
                      activeIdx == 14);
      // World & Scene — shared hub for HUD / player visibility, time and
      // weather. Replaces the old inline Hide HUD + Hide Player rows so the
      // same controls (and now time/weather, previously unreachable from
      // Sequence mode) live in one place across both camera modes.
      if (vis(15))
        DrawMenuValue("World & Scene...", "", lineWidth, 9.0, rowTop(15), 0.0,
                      9.0, activeIdx == 15);
      // Render to Image Sequence — value shows the frame count at the current
      // output fps so the user knows the size before committing.
      if (vis(16)) {
        char renderVal[48];
        int frames = (int)(Sequence_TotalDuration() * g_RenderFps + 0.5f);
        sprintf_s(renderVal, "%d fps  -  %d frames", (int)g_RenderFps, frames);
        DrawMenuValue("Render to Images...", renderVal, lineWidth, 9.0,
                      rowTop(16), 0.0, 9.0, activeIdx == 16);
      }
      if (vis(17))
        DrawMenuValue("Exit", "Mode picker", lineWidth, 9.0,
                      rowTop(17), 0.0, 9.0, activeIdx == 17);

      // Drive the sequence mode every frame while this menu is open.
      // script.cpp's main loop is blocked at ProcessConfigMenu(), so it
      // can't tick anything — we have to do it ourselves here, just like
      // every Free Camera submenu calls UpdateFreeCamera() in its draw
      // loop. This is what lets the user free-fly the camera while the
      // sequence menu is open AND lets playback animate live.
      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack || IsMenuTogglePressed()) {
      MenuBeep();
      return false;
    }
    if (bUp)   { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    CameraSequence *s = Sequence_Active();

    if (bSelect) {
      MenuBeep();
      switch (activeIdx) {
      case 0: // Sequence cycle: Enter creates new
        Sequence_New("Untitled");
        SetStatusText("New sequence created");
        break;
      case 1: Sequence_TogglePlay();
        SetStatusText(Sequence_IsPlaying() ? "Playing" : "Paused"); break;
      case 2: Sequence_Stop(); SetStatusText("Stopped"); break;
      case 3: if (s) { s->loop = !s->loop;
                       SetStatusText(s->loop ? "Loop on" : "Loop off"); } break;
      case 4: {
        if (s) {
          float v;
          if (PromptForFloat(v, s->playbackSpeed)) {
            if (v < 0.05f) v = 0.05f;
            if (v > 8.0f) v = 8.0f;
            s->playbackSpeed = v;
          }
        }
        break;
      }
      case 5: {
        float v;
        if (PromptForFloat(v, Sequence_CurrentTime())) {
          Sequence_SetCurrentTime(v);
        }
        break;
      }
      case 6: ProcessPoseListMenu(); break;
      case 7: ProcessEventListMenu(); break;
      case 8: {
        int idx = Sequence_CapturePoseAtCurrentTime();
        if (idx >= 0) SetStatusText("Pose captured");
        break;
      }
      case 9: Sequence_New("Untitled"); SetStatusText("New sequence"); break;
      case 10:
        if (GetTickCount() < deleteArmedUntil) {
          Sequence_DeleteActive();
          deleteArmedUntil = 0;
          SetStatusText("Sequence deleted");
        } else {
          deleteArmedUntil = GetTickCount() + 3000;
          SetStatusText("Press Enter again to confirm delete");
        }
        break;
      case 11: Sequence_SaveAll(); SetStatusText("Saved to INI"); break;
      case 12: {
        int idx = Sequence_CloseLoop();
        if (idx >= 0) {
          SetStatusText(Sequence_IsLoopClosed() ? "Loop closed"
                                                : "Closing keyframe added");
        }
        break;
      }
      case 13:
        ProcessSequenceFollowMenu();
        break;
      case 14:
        g_SequenceShowMarkers = !g_SequenceShowMarkers;
        SetStatusText(g_SequenceShowMarkers ? "Markers visible"
                                            : "Markers hidden");
        break;
      case 15:
        ProcessWorldSceneMenu();
        break;
      case 16:
        ProcessRenderMenu();
        break;
      case 17: return true; // Exit → return to mode picker
      }
      waitTime = 200;
    }

    // Speed (4) and Time (5) are numeric and accelerate on hold; the sequence
    // cycle (0) and Loop toggle (3) keep a fixed repeat.
    bool numericRow = (activeIdx == 4 || activeIdx == 5);
    DWORD adjWait = 100;
    float mult = HoldAccel((bRight || bLeft) && numericRow, holdStart,
                           wasHolding, &adjWait);

    if (bRight) {
      MenuBeep();
      if (activeIdx == 0) { // cycle sequences
        if (seqIdx < seqCount - 1) Sequence_SetActive(seqIdx + 1);
      } else if (activeIdx == 3 && s) {
        s->loop = !s->loop;
      } else if (activeIdx == 4 && s) {
        s->playbackSpeed += 0.1f * mult;
        if (s->playbackSpeed > 8.0f) s->playbackSpeed = 8.0f;
      } else if (activeIdx == 5) {
        Sequence_SetCurrentTime(Sequence_CurrentTime() + 0.1f * mult);
      }
      waitTime = adjWait;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 0) {
        if (seqIdx > 0) Sequence_SetActive(seqIdx - 1);
      } else if (activeIdx == 3 && s) {
        s->loop = !s->loop;
      } else if (activeIdx == 4 && s) {
        s->playbackSpeed -= 0.1f * mult;
        if (s->playbackSpeed < 0.05f) s->playbackSpeed = 0.05f;
      } else if (activeIdx == 5) {
        Sequence_SetCurrentTime(Sequence_CurrentTime() - 0.1f * mult);
      }
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Follow & Entity Lock — submenu under Camera Sequence
// ============================================================
//
// Self-contained version of the follow/lock controls from the Free
// Camera Movement menu, plus two sequence-specific batch actions
// (Apply Lock to All Keyframes / Clear All Locks). Lives in the
// Sequence main menu so the user can compose entity-locked shots
// without leaving Sequence mode.
//
// The dynamic row layout mirrors the Free Camera version: rows below
// Follow Target only appear when relevant (Lock Entity when Aimed mode
// is on; Rigid + Marker only when an entity is actually locked). The
// batch actions sit at the bottom and are always present so the user
// can wipe locks even when the free-cam currently has no target.

static void ProcessSequenceFollowMenu() {
  const float lineWidth = 360.0f;
  int activeIdx = 0;

  DWORD waitTime = 150;
  while (true) {
    // "Player's vehicle" row is only meaningful while the player ped is
    // actually in a vehicle — peek the natives so we can hide the row
    // (and its shortcut) when there's nothing to lock to. Computed once
    // per outer iteration so render + input passes agree on the layout.
    int playerVeh = 0;
    {
      Ped pp = PLAYER::PLAYER_PED_ID();
      if (PED::IS_PED_IN_ANY_VEHICLE(pp, FALSE)) {
        playerVeh = PED::GET_VEHICLE_PED_IS_IN(pp, FALSE);
        if (!ENTITY::DOES_ENTITY_EXIST(playerVeh)) playerVeh = 0;
      }
    }

    // "Has lock target" — the union of Player follow (always has a
    // target: the player ped) and Aimed Entity with a locked handle.
    // Both can drive keyframe locks, so the batch / Rigid / Marker rows
    // treat them uniformly.
    bool hasLockTarget = (g_FollowMode == 1) ||
                         (g_FollowMode == 2 && g_FollowTargetEntity != 0);

    // Dynamic row count.
    //  Mode 1 (Player):
    //    0  Follow Target
    //    1  Rigid Mode
    //    2  Show Marker
    //    3..5 Batch actions
    //
    //  Mode 2 (Aimed):
    //    0  Follow Target
    //    1  Lock Entity (raycast)
    //    2  Lock to Nearest Vehicle
    //   [3] Lock to Player's Vehicle  (only when player is in one)
    //   [N] Rigid Mode + Show Marker  (only when locked)
    //    .. Batch actions
    //
    // The two "Lock to X" rows are foolproof fallbacks for the raycast,
    // which fails when the camera is inside the target's collision volume
    // (close-up vehicle shots, especially). The raycast row stays at the
    // top because it's the most precise option when it works.
    int lineCount = 1 + 3; // Follow Target + 3 batch actions
    if (g_FollowMode == 1) {
      lineCount += 2; // Rigid + Marker (player ped is the implicit target)
    } else if (g_FollowMode == 2) {
      lineCount += 1; // Lock Entity (raycast)
      lineCount += 1; // Lock to Nearest Vehicle
      if (playerVeh != 0) lineCount += 1; // Lock to Player's Vehicle
      if (g_FollowTargetEntity != 0) lineCount += 2; // Rigid + Marker
    }

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();

      char title[64];
      sprintf_s(title, "FOLLOW & ENTITY LOCK");
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Follow Target row — shared with the Movement menu's display so
      // the user sees consistent labels everywhere.
      std::string followStr = FollowModeName(g_FollowMode);
      DrawMenuValue("Follow Target", followStr, lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);

      int row = 1;
      int lockIdx = -1, lockNearestIdx = -1, lockPlayerVehIdx = -1;
      int rigidIdx = -1, markerIdx = -1;
      if (g_FollowMode == 1) {
        // Player follow: ped is the implicit target. Just expose the
        // two togglable behaviors that apply to it.
        rigidIdx = row;
        DrawMenuValue("  - Rigid Mode", FormatBool(g_FollowRigidMode),
                      lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                      activeIdx == row);
        row++;
        markerIdx = row;
        DrawMenuValue("  - Show Marker", FormatBool(g_ShowLockedEntityMarker),
                      lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                      activeIdx == row);
        row++;
      } else if (g_FollowMode == 2) {
        lockIdx = row;
        std::string entityStr = (g_FollowTargetEntity == 0)
                                    ? "Aim at entity, Enter to lock"
                                    : "Locked - Enter to release";
        DrawMenuValue("  - Lock Entity (raycast)", entityStr, lineWidth, 9.0,
                      60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;
        // Two raycast-free fallbacks. Useful because:
        //  - Cars have chunky collision; if the camera is inside the
        //    bounding volume (close-up shots), START_SHAPE_TEST_RAY can
        //    return no hit. GET_CLOSEST_VEHICLE works regardless.
        //  - "Lock to Player's Vehicle" is the common case for in-car
        //    cinematic shots — one click and the camera tracks the car
        //    the player is driving.
        lockNearestIdx = row;
        DrawMenuValue("  - Lock to Nearest Vehicle", "Press Enter", lineWidth,
                      9.0, 60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;
        if (playerVeh != 0) {
          lockPlayerVehIdx = row;
          DrawMenuValue("  - Lock to Player's Vehicle", "Press Enter",
                        lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                        activeIdx == row);
          row++;
        }
        if (g_FollowTargetEntity != 0) {
          rigidIdx = row;
          DrawMenuValue("  - Rigid Mode", FormatBool(g_FollowRigidMode),
                        lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                        activeIdx == row);
          row++;
          markerIdx = row;
          DrawMenuValue("  - Show Marker", FormatBool(g_ShowLockedEntityMarker),
                        lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                        activeIdx == row);
          row++;
        }

        // Hover marker: while in Aimed mode with no target yet, fire the
        // same raycast the Movement menu uses and draw a marker on the
        // entity under the crosshair so the user can see what they're
        // about to lock.
        if (g_FollowTargetEntity == 0) {
          DrawEntityHoverMarker(RaycastEntityFromCamera(30));
        }
      }

      // Batch action rows always present.
      int applyAllIdx     = row++;
      int clearAllIdx     = row++;
      int recaptureAllIdx = row++;

      int lockedCount = Sequence_LockedPoseCount();
      CameraSequence *seq = Sequence_Active();
      int totalPoses = seq ? (int)seq->poses.size() : 0;
      char applyVal[64], clearVal[64];
      sprintf_s(applyVal, "%s",
                hasLockTarget ? "Press Enter" : "(lock free-cam first)");
      sprintf_s(clearVal, "%d / %d locked", lockedCount, totalPoses);
      DrawMenuValue("Apply Lock to All Keyframes", applyVal, lineWidth, 9.0,
                    60.0 + applyAllIdx * 36.0, 0.0, 9.0,
                    activeIdx == applyAllIdx);
      DrawMenuValue("Clear All Keyframe Locks", clearVal, lineWidth, 9.0,
                    60.0 + clearAllIdx * 36.0, 0.0, 9.0,
                    activeIdx == clearAllIdx);
      // "Recapture World Coords" bakes the entity's current world
      // position into the keyframe's world-space fallback. Useful right
      // before clearing locks if the user wants to "freeze in place"
      // (otherwise the fallback coords still point to where the entity
      // was when the keyframe was originally authored).
      DrawMenuValue("Bake Locked Poses to World", "Press Enter", lineWidth,
                    9.0, 60.0 + recaptureAllIdx * 36.0, 0.0, 9.0,
                    activeIdx == recaptureAllIdx);

      // Tick playback / free-cam every frame so the user can compose
      // shots while this menu is open (same pattern as ProcessPoseListMenu).
      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    // Recompute row indices for the input pass — the draw pass clobbers
    // the locals once it exits the do-while above. Keep this in lockstep
    // with the same layout used for rendering above.
    int row2 = 1;
    int lockIdx2 = -1, lockNearestIdx2 = -1, lockPlayerVehIdx2 = -1;
    int rigidIdx2 = -1, markerIdx2 = -1;
    if (g_FollowMode == 1) {
      rigidIdx2 = row2++;
      markerIdx2 = row2++;
    } else if (g_FollowMode == 2) {
      lockIdx2 = row2++;
      lockNearestIdx2 = row2++;
      if (playerVeh != 0) lockPlayerVehIdx2 = row2++;
      if (g_FollowTargetEntity != 0) {
        rigidIdx2 = row2++;
        markerIdx2 = row2++;
      }
    }
    int applyAllIdx2 = row2++;
    int clearAllIdx2 = row2++;
    int recaptureAllIdx2 = row2++;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) { MenuBeep(); return; }
    if (bUp)   { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == 0) {
        // Enter cycles follow mode the same way the Movement menu's
        // right-arrow does. Player follow is skipped when Move Player
        // is on (would create a recursive lock — same guard as Movement).
        g_FollowMode = (g_FollowMode + 1) % 3;
        if (g_FollowMode == 1 && g_MovePlayerWithCamera) g_FollowMode = 2;
        if (g_FollowMode != 2) g_FollowTargetEntity = 0;
        const char *names[3] = {"Follow: None", "Follow: Player",
                                "Follow: Aimed Entity"};
        SetStatusText(names[g_FollowMode]);
      } else if (lockIdx2 != -1 && activeIdx == lockIdx2) {
        if (g_FollowTargetEntity != 0) {
          g_FollowTargetEntity = 0;
          SetStatusText("Entity unlocked");
        } else {
          // Raycast forward from the camera and lock onto whatever the
          // user is aiming at. Flags 30 = vehicles|peds|ragdolls|objects.
          int e = RaycastEntityFromCamera(30);
          if (e != 0) {
            g_FollowTargetEntity = e;
            SetStatusText("Locked onto entity");
          } else {
            g_FollowTargetEntity = 0;
            SetStatusText("No entity found. Aim closer.");
          }
        }
      } else if (lockNearestIdx2 != -1 && activeIdx == lockNearestIdx2) {
        // GET_CLOSEST_VEHICLE search around the current camera position.
        // 30m radius is roughly "anything that's clearly in shot"; bigger
        // would risk locking to a vehicle behind the user. modelHash=0
        // means any model. Flags 70 = 2|4|64 — civilian + already-spawned
        // + don't include trains; the de-facto "any normal vehicle" combo
        // used widely in scripts.
        float posX, posY, posZ, pitch, yaw, roll;
        GetCameraState(posX, posY, posZ, pitch, yaw, roll);
        int veh = VEHICLE::GET_CLOSEST_VEHICLE(posX, posY, posZ, 30.0f, 0, 70);
        if (veh != 0 && ENTITY::DOES_ENTITY_EXIST(veh)) {
          g_FollowTargetEntity = veh;
          SetStatusText("Locked onto nearest vehicle");
        } else {
          SetStatusText("No vehicle within 30m");
        }
      } else if (lockPlayerVehIdx2 != -1 && activeIdx == lockPlayerVehIdx2) {
        // Recomputed here rather than reusing playerVeh — between the row
        // layout pass and this input pass the player could have exited
        // the vehicle. Defensive recheck keeps the lock honest.
        Ped pp = PLAYER::PLAYER_PED_ID();
        int veh = PED::IS_PED_IN_ANY_VEHICLE(pp, FALSE)
                      ? PED::GET_VEHICLE_PED_IS_IN(pp, FALSE)
                      : 0;
        if (veh != 0 && ENTITY::DOES_ENTITY_EXIST(veh)) {
          g_FollowTargetEntity = veh;
          SetStatusText("Locked onto player's vehicle");
        } else {
          SetStatusText("Player isn't in a vehicle");
        }
      } else if (rigidIdx2 != -1 && activeIdx == rigidIdx2) {
        g_FollowRigidMode = !g_FollowRigidMode;
        SetStatusText(g_FollowRigidMode ? "Rigid mode on (cam rotates with entity)"
                                        : "Rigid mode off");
      } else if (markerIdx2 != -1 && activeIdx == markerIdx2) {
        g_ShowLockedEntityMarker = !g_ShowLockedEntityMarker;
        SetStatusText(g_ShowLockedEntityMarker ? "Marker visible"
                                               : "Marker hidden");
      } else if (activeIdx == applyAllIdx2) {
        // Resolve the current free-cam target. Player follow → player
        // ped; Aimed mode → the locked handle; nothing → bail.
        int target = 0;
        if (g_FollowMode == 1)      target = PLAYER::PLAYER_PED_ID();
        else if (g_FollowMode == 2) target = g_FollowTargetEntity;
        if (target == 0 || !ENTITY::DOES_ENTITY_EXIST(target)) {
          SetStatusText("Lock free-cam to a target first");
        } else {
          int n = Sequence_ApplyLockToAll(target);
          char msg[64];
          sprintf_s(msg, "Locked %d keyframes to current target", n);
          SetStatusText(msg);
        }
      } else if (activeIdx == clearAllIdx2) {
        int n = Sequence_ClearAllLocks();
        char msg[64];
        sprintf_s(msg, "Cleared lock from %d keyframes", n);
        SetStatusText(msg);
      } else if (activeIdx == recaptureAllIdx2) {
        // For every locked keyframe whose entity still exists, write
        // the current world position back into the world-space coords.
        // Effectively: snapshot the lock to a static world position so
        // the keyframe still works after the entity is gone. Pairs well
        // with "Clear All Locks" right after for a permanent bake.
        CameraSequence *cs = Sequence_Active();
        int n = 0;
        if (cs) {
          for (PoseKeyframe &p : cs->poses) {
            if (p.entityHandle != 0 &&
                ENTITY::DOES_ENTITY_EXIST(p.entityHandle)) {
              Vector3 w = invoke<Vector3>(0x1899F328B0E12848, p.entityHandle,
                                          p.localOffsetX, p.localOffsetY,
                                          p.localOffsetZ);
              p.posX = w.x; p.posY = w.y; p.posZ = w.z;
              ++n;
            }
          }
        }
        char msg[64];
        sprintf_s(msg, "Baked %d locked poses to world coords", n);
        SetStatusText(msg);
      }
      waitTime = 200;
    }

    // Left/right cycles the Follow Target same as Enter (matches
    // Movement menu UX). The two boolean rows toggle on left/right too.
    if (bRight || bLeft) {
      MenuBeep();
      if (activeIdx == 0) {
        if (bRight) {
          g_FollowMode = (g_FollowMode + 1) % 3;
          if (g_FollowMode == 1 && g_MovePlayerWithCamera) g_FollowMode = 2;
        } else {
          g_FollowMode = (g_FollowMode == 0) ? 2 : g_FollowMode - 1;
          if (g_FollowMode == 1 && g_MovePlayerWithCamera) g_FollowMode = 0;
        }
        if (g_FollowMode != 2) g_FollowTargetEntity = 0;
      } else if (rigidIdx2 != -1 && activeIdx == rigidIdx2) {
        g_FollowRigidMode = !g_FollowRigidMode;
      } else if (markerIdx2 != -1 && activeIdx == markerIdx2) {
        g_ShowLockedEntityMarker = !g_ShowLockedEntityMarker;
      }
      waitTime = 100;
    }
  }
}

// ============================================================
//  Pose Keyframes — list submenu
// ============================================================

static void ProcessPoseListMenu() {
  const float lineWidth = 360.0f;
  int activeIdx = 0;
  // Scroll offset survives the inner render/input loop iterations so the
  // viewport doesn't jump when the cursor wraps around the list.
  int scrollOffset = 0;
  // Hold-to-accelerate state for the "Scale All Times" row.
  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    CameraSequence *s = Sequence_Active();
    int poseCount = s ? (int)s->poses.size() : 0;
    // Row 0 = "Scale All Times"; rows 1..poseCount = keyframes; last = Add.
    int lineCount = poseCount + 2;
    if (activeIdx >= lineCount) activeIdx = lineCount - 1;
    if (activeIdx < 0) activeIdx = 0;
    EnsureRowVisible(activeIdx, lineCount, scrollOffset);

    int visEnd = scrollOffset + kMaxVisibleListRows;
    if (visEnd > lineCount) visEnd = lineCount;

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      char title[64];
      if (lineCount > kMaxVisibleListRows) {
        sprintf_s(title, "POSE KEYFRAMES  [%d-%d / %d]", scrollOffset + 1,
                  visEnd, lineCount);
      } else {
        sprintf_s(title, "POSE KEYFRAMES");
      }
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      for (int i = scrollOffset; i < visEnd; ++i) {
        int visRow = i - scrollOffset;
        float top = 60.0f + visRow * 36.0f;
        if (i == 0) {
          // Scale All Times — left/right stretches/compresses every keyframe
          // (and event) time at once. Value shows the resulting duration.
          char val[64];
          sprintf_s(val, "total %.2fs   <- / ->", Sequence_TotalDuration());
          DrawMenuValue("Scale All Times", val, lineWidth, 9.0, top, 0.0, 9.0,
                        activeIdx == 0);
        } else if (i <= poseCount) {
          int pi = i - 1;
          const PoseKeyframe &p = s->poses[pi];
          char label[64], val[96];
          sprintf_s(label, "KF %d", pi);
          sprintf_s(val, "t=%.2fs  fov=%.0f  %s/%s", p.t, p.fov,
                    EaseName(p.ease), PathName(p.path));
          DrawMenuValue(label, val, lineWidth, 9.0, top, 0.0, 9.0,
                        activeIdx == i);
        } else {
          DrawMenuValue("+ Add at current camera pose", "F6", lineWidth, 9.0,
                        top, 0.0, 9.0, activeIdx == i);
        }
      }

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) { MenuBeep(); return; }
    if (bUp)   { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == 0) {
        SetStatusText("Use Left/Right to stretch or compress all keyframe times");
      } else if (activeIdx <= poseCount) {
        ProcessPoseEditMenu(activeIdx - 1);
      } else {
        int newIdx = Sequence_CapturePoseAtCurrentTime();
        if (newIdx >= 0) SetStatusText("Pose captured");
      }
      waitTime = 200;
    }

    // Left/right on the Scale row rewrites every keyframe + event time. Right
    // stretches (slower / longer), left compresses (faster / shorter); inverse
    // factors so they undo each other. Hold-to-accelerate for big changes.
    DWORD adjWait = 100;
    float mult = HoldAccel((bRight || bLeft) && activeIdx == 0, holdStart,
                           wasHolding, &adjWait);
    if (activeIdx == 0 && (bRight || bLeft)) {
      MenuBeep();
      float step = 1.0f + 0.02f * mult;
      Sequence_ScaleTimes(bRight ? step : (1.0f / step));
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Pose Keyframe — per-item editor
// ============================================================

static void ProcessPoseEditMenu(int poseIdx) {
  const float lineWidth = 320.0f;
  // 13 rows: Time, Pos X/Y/Z, Pitch, Yaw, Roll, FOV, Ease, Path,
  //          Entity Lock, Recapture, Delete.
  const int lineCount = 13;
  // Row indices (kept as locals so adding rows in the future only needs
  // updating this block + the corresponding case statements below).
  const int IDX_TIME = 0;
  const int IDX_POSX = 1, IDX_POSY = 2, IDX_POSZ = 3;
  const int IDX_PITCH = 4, IDX_YAW = 5, IDX_ROLL = 6;
  const int IDX_FOV = 7;
  const int IDX_EASE = 8, IDX_PATH = 9;
  const int IDX_LOCK = 10;
  const int IDX_RECAPTURE = 11;
  const int IDX_DELETE = 12;
  int activeIdx = 0;
  // Mark this pose as being edited so DrawSequenceMarkers paints its
  // in-world sphere distinctly. The RAII guard clears the marker on
  // every return path (bBack, delete, validity bail-outs) so we don't
  // leak the highlight after the editor closes.
  Sequence_SetEditingPose(poseIdx);
  struct ClearEditGuard { ~ClearEditGuard() { Sequence_SetEditingPose(-1); } } _g;

  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    CameraSequence *s = Sequence_Active();
    if (!s || poseIdx < 0 || poseIdx >= (int)s->poses.size()) return;
    PoseKeyframe &p = s->poses[poseIdx];

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      char title[32]; sprintf_s(title, "KEYFRAME %d", poseIdx);
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      DrawMenuValue("Time (s)", FormatFloat(p.t, 2), lineWidth, 9.0,
                    60.0 + IDX_TIME * 36.0, 0.0, 9.0, activeIdx == IDX_TIME);
      DrawMenuValue("Pos X", FormatFloat(p.posX, 2), lineWidth, 9.0,
                    60.0 + IDX_POSX * 36.0, 0.0, 9.0, activeIdx == IDX_POSX);
      DrawMenuValue("Pos Y", FormatFloat(p.posY, 2), lineWidth, 9.0,
                    60.0 + IDX_POSY * 36.0, 0.0, 9.0, activeIdx == IDX_POSY);
      DrawMenuValue("Pos Z", FormatFloat(p.posZ, 2), lineWidth, 9.0,
                    60.0 + IDX_POSZ * 36.0, 0.0, 9.0, activeIdx == IDX_POSZ);
      DrawMenuValue("Pitch", FormatFloat(p.pitch, 2), lineWidth, 9.0,
                    60.0 + IDX_PITCH * 36.0, 0.0, 9.0, activeIdx == IDX_PITCH);
      DrawMenuValue("Yaw", FormatFloat(p.yaw, 2), lineWidth, 9.0,
                    60.0 + IDX_YAW * 36.0, 0.0, 9.0, activeIdx == IDX_YAW);
      DrawMenuValue("Roll", FormatFloat(p.roll, 2), lineWidth, 9.0,
                    60.0 + IDX_ROLL * 36.0, 0.0, 9.0, activeIdx == IDX_ROLL);
      DrawMenuValue("FOV", FormatFloat(p.fov, 1), lineWidth, 9.0,
                    60.0 + IDX_FOV * 36.0, 0.0, 9.0, activeIdx == IDX_FOV);
      DrawMenuValue("Ease", EaseName(p.ease), lineWidth, 9.0,
                    60.0 + IDX_EASE * 36.0, 0.0, 9.0, activeIdx == IDX_EASE);
      DrawMenuValue("Path", PathName(p.path), lineWidth, 9.0,
                    60.0 + IDX_PATH * 36.0, 0.0, 9.0, activeIdx == IDX_PATH);
      // Entity Lock value cycles through four readable states. The
      // distinction between "Locked" and "Locked (entity lost)" is the
      // load-time clearing of entityHandle: a saved keyframe that was
      // authored locked comes back with the offsets intact but handle=0,
      // so playback falls through to world coords. Surfacing that state
      // lets the user know the keyframe wants to be re-anchored.
      const char *lockStr;
      if (p.entityHandle != 0) {
        if (ENTITY::DOES_ENTITY_EXIST(p.entityHandle)) lockStr = "Locked";
        else                                           lockStr = "Locked (entity gone)";
      } else if (p.localOffsetX != 0.0f || p.localOffsetY != 0.0f ||
                 p.localOffsetZ != 0.0f || p.lockEntPitch != 0.0f ||
                 p.lockEntYaw != 0.0f || p.lockEntRoll != 0.0f) {
        lockStr = "Authored locked";
      } else {
        lockStr = "None";
      }
      DrawMenuValue("Entity Lock", lockStr, lineWidth, 9.0,
                    60.0 + IDX_LOCK * 36.0, 0.0, 9.0, activeIdx == IDX_LOCK);
      DrawMenuValue("Recapture from live", "Press Enter", lineWidth, 9.0,
                    60.0 + IDX_RECAPTURE * 36.0, 0.0, 9.0, activeIdx == IDX_RECAPTURE);
      DrawMenuValue("Delete", "Press Enter", lineWidth, 9.0,
                    60.0 + IDX_DELETE * 36.0, 0.0, 9.0, activeIdx == IDX_DELETE);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) { MenuBeep(); Sequence_SortByTime(); return; }
    if (bUp)   { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    // Re-fetch pointer in case insert/delete shifted memory
    s = Sequence_Active();
    if (!s || poseIdx >= (int)s->poses.size()) return;
    PoseKeyframe &pose = s->poses[poseIdx];

    auto promptFloat = [&](float &target) {
      float v;
      if (PromptForFloat(v, target)) target = v;
    };

    if (bSelect) {
      MenuBeep();
      if      (activeIdx == IDX_TIME)  promptFloat(pose.t);
      else if (activeIdx == IDX_POSX)  promptFloat(pose.posX);
      else if (activeIdx == IDX_POSY)  promptFloat(pose.posY);
      else if (activeIdx == IDX_POSZ)  promptFloat(pose.posZ);
      else if (activeIdx == IDX_PITCH) promptFloat(pose.pitch);
      else if (activeIdx == IDX_YAW)   promptFloat(pose.yaw);
      else if (activeIdx == IDX_ROLL)  promptFloat(pose.roll);
      else if (activeIdx == IDX_FOV)   promptFloat(pose.fov);
      else if (activeIdx == IDX_EASE)  pose.ease = (EaseType)(((int)pose.ease + 1) % 5);
      else if (activeIdx == IDX_PATH)  pose.path = (PathType)(((int)pose.path + 1) % 2);
      else if (activeIdx == IDX_LOCK) {
        // Three-way toggle on Enter:
        //   - Currently locked         → clear (handle=0 + offsets=0)
        //   - Has stored offsets, no
        //     handle (loaded from INI) → try to re-lock to current free-
        //     cam target; if none, just clear the stale offsets
        //   - Unlocked                 → try to lock to current free-cam
        //     target; if none, status-message the user
        if (pose.entityHandle != 0) {
          pose.entityHandle = 0;
          pose.localOffsetX = pose.localOffsetY = pose.localOffsetZ = 0.0f;
          pose.lockEntPitch = pose.lockEntYaw = pose.lockEntRoll = 0.0f;
          SetStatusText("Entity lock cleared");
        } else if ((g_FollowMode == 1) ||
                   (g_FollowMode == 2 && g_FollowTargetEntity != 0 &&
                    ENTITY::DOES_ENTITY_EXIST(g_FollowTargetEntity))) {
          // Player follow OR Aimed Entity with a target — both produce
          // a valid keyframe lock via CaptureLockForPose.
          Sequence_CaptureLockForPose(pose);
          if (pose.entityHandle != 0) SetStatusText("Locked to free-cam target");
          else                        SetStatusText("Lock failed");
        } else {
          // Authored-but-stale offsets: zero them so the value column
          // doesn't keep advertising a lock that can't be re-applied
          // without a free-cam target.
          pose.localOffsetX = pose.localOffsetY = pose.localOffsetZ = 0.0f;
          pose.lockEntPitch = pose.lockEntYaw = pose.lockEntRoll = 0.0f;
          SetStatusText("Aim & lock in Free Camera first, then try again");
        }
      }
      else if (activeIdx == IDX_RECAPTURE) {
        float px, py, pz, pitch, yaw, roll;
        GetCameraState(px, py, pz, pitch, yaw, roll);
        pose.posX = px; pose.posY = py; pose.posZ = pz;
        pose.pitch = pitch; pose.yaw = yaw; pose.roll = roll;
        pose.fov = g_CamFOV;
        // Refresh the entity-lock snapshot too. If free-cam is currently
        // locked, this picks up the new offset; if not, it clears any
        // stale lock fields (matching the live-camera state exactly).
        Sequence_CaptureLockForPose(pose);
        SetStatusText("Pose recaptured from live camera");
      }
      else if (activeIdx == IDX_DELETE) {
        Sequence_DeletePose(poseIdx);
        SetStatusText("Keyframe deleted");
        return; // index is now stale; pop back to list
      }
      waitTime = 200;
    }

    // Left/right: nudge numeric values; cycle enums. Entity Lock, Recapture
    // and Delete rows ignore left/right — they're Enter-activated actions,
    // not nudgeable values. The numeric rows (Time..FOV) accelerate on hold;
    // the Ease / Path enums keep a fixed repeat.
    bool numericRow = (activeIdx >= IDX_TIME && activeIdx <= IDX_FOV);
    DWORD adjWait = 100;
    float mult = HoldAccel((bRight || bLeft) && numericRow, holdStart,
                           wasHolding, &adjWait);
    if (bRight) {
      MenuBeep();
      if      (activeIdx == IDX_TIME)  { pose.t += 0.1f * mult; if (pose.t < 0.0f) pose.t = 0.0f; }
      else if (activeIdx == IDX_POSX)  pose.posX += 0.5f * mult;
      else if (activeIdx == IDX_POSY)  pose.posY += 0.5f * mult;
      else if (activeIdx == IDX_POSZ)  pose.posZ += 0.5f * mult;
      else if (activeIdx == IDX_PITCH) pose.pitch += 1.0f * mult;
      else if (activeIdx == IDX_YAW)   pose.yaw += 1.0f * mult;
      else if (activeIdx == IDX_ROLL)  pose.roll += 1.0f * mult;
      else if (activeIdx == IDX_FOV)   { pose.fov += 1.0f * mult; if (pose.fov > 130.0f) pose.fov = 130.0f; }
      else if (activeIdx == IDX_EASE)  pose.ease = (EaseType)(((int)pose.ease + 1) % 5);
      else if (activeIdx == IDX_PATH)  pose.path = (PathType)(((int)pose.path + 1) % 2);
      waitTime = adjWait;
    }
    if (bLeft) {
      MenuBeep();
      if      (activeIdx == IDX_TIME)  { pose.t -= 0.1f * mult; if (pose.t < 0.0f) pose.t = 0.0f; }
      else if (activeIdx == IDX_POSX)  pose.posX -= 0.5f * mult;
      else if (activeIdx == IDX_POSY)  pose.posY -= 0.5f * mult;
      else if (activeIdx == IDX_POSZ)  pose.posZ -= 0.5f * mult;
      else if (activeIdx == IDX_PITCH) pose.pitch -= 1.0f * mult;
      else if (activeIdx == IDX_YAW)   pose.yaw -= 1.0f * mult;
      else if (activeIdx == IDX_ROLL)  pose.roll -= 1.0f * mult;
      else if (activeIdx == IDX_FOV)   { pose.fov -= 1.0f * mult; if (pose.fov < 5.0f) pose.fov = 5.0f; }
      else if (activeIdx == IDX_EASE)  pose.ease = (EaseType)(((int)pose.ease + 4) % 5);
      else if (activeIdx == IDX_PATH)  pose.path = (PathType)(((int)pose.path + 1) % 2);
      waitTime = adjWait;
    }
  }
}

// ============================================================
//  Effect Events — list submenu
// ============================================================

static void ProcessEventListMenu() {
  const float lineWidth = 360.0f;
  int activeIdx = 0;
  // See ProcessPoseListMenu — viewport pattern.
  int scrollOffset = 0;

  DWORD waitTime = 150;
  while (true) {
    CameraSequence *s = Sequence_Active();
    int evCount = s ? (int)s->events.size() : 0;
    int lineCount = evCount + 1; // + "Add event"
    if (activeIdx >= lineCount) activeIdx = lineCount - 1;
    if (activeIdx < 0) activeIdx = 0;
    EnsureRowVisible(activeIdx, lineCount, scrollOffset);

    int visEnd = scrollOffset + kMaxVisibleListRows;
    if (visEnd > lineCount) visEnd = lineCount;

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      char title[64];
      if (lineCount > kMaxVisibleListRows) {
        sprintf_s(title, "EFFECT EVENTS  [%d-%d / %d]", scrollOffset + 1,
                  visEnd, lineCount);
      } else {
        sprintf_s(title, "EFFECT EVENTS");
      }
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Same approach as ProcessPoseListMenu — title bar carries the
      // X-Y / total range info; no separate "more above/below" rows.
      for (int i = scrollOffset; i < visEnd; ++i) {
        int visRow = i - scrollOffset;
        float top = 60.0f + visRow * 36.0f;
        if (i < evCount) {
          const EffectEvent &e = s->events[i];
          char label[64], val[96];
          sprintf_s(label, "EV %d", i);
          sprintf_s(val, "t=%.2fs  %s=%.2f  %s", e.t, EffectName(e.kind),
                    e.value, e.ramp ? "ramp" : "snap");
          DrawMenuValue(label, val, lineWidth, 9.0, top, 0.0, 9.0,
                        activeIdx == i);
        } else {
          DrawMenuValue("+ Add event at current time", "Press Enter",
                        lineWidth, 9.0, top, 0.0, 9.0, activeIdx == i);
        }
      }

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) { MenuBeep(); return; }
    if (bUp)   { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    if (bSelect) {
      MenuBeep();
      if (activeIdx == evCount) {
        // Add new event with current time, default kind = Shake On/Off = 1.
        // Sequence_AddEvent sorts after insertion, so we have to look up
        // the new event's index by its (time, kind, value) signature to
        // open the editor on the right row.
        float t = Sequence_CurrentTime();
        Sequence_AddEvent(EFX_SHAKE_ENABLED, t, 1.0f, false);
        CameraSequence *s2 = Sequence_Active();
        int newIdx = -1;
        if (s2) {
          for (int i = 0; i < (int)s2->events.size(); ++i) {
            const EffectEvent &ev = s2->events[i];
            if (ev.kind == EFX_SHAKE_ENABLED && fabsf(ev.t - t) < 0.001f &&
                ev.value == 1.0f && !ev.ramp) {
              newIdx = i;
              break;
            }
          }
        }
        if (newIdx >= 0) {
          // Drop the user straight into the editor for the new event so
          // they can pick the kind / value without hunting in the list.
          ProcessEventEditMenu(newIdx);
        } else {
          SetStatusText("Event added");
        }
      } else {
        ProcessEventEditMenu(activeIdx);
      }
      waitTime = 200;
    }
  }
}

// ============================================================
//  Effect Event — per-item editor
// ============================================================

static void ProcessEventEditMenu(int eventIdx) {
  const float lineWidth = 320.0f;
  const int lineCount = 5;
  int activeIdx = 0;

  DWORD holdStart = 0;
  bool wasHolding = false;

  DWORD waitTime = 150;
  while (true) {
    CameraSequence *s = Sequence_Active();
    if (!s || eventIdx < 0 || eventIdx >= (int)s->events.size()) return;
    EffectEvent &e = s->events[eventIdx];

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      char title[32]; sprintf_s(title, "EVENT %d", eventIdx);
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      DrawMenuValue("Time (s)", FormatFloat(e.t, 2), lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Effect", EffectName(e.kind), lineWidth, 9.0,
                    60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      DrawMenuValue("Value", FormatFloat(e.value, 3), lineWidth, 9.0,
                    60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Mode", e.ramp ? "Ramp (lerp from prev)" : "Snap",
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Delete", "Press Enter", lineWidth, 9.0, 60.0 + 4 * 36.0,
                    0.0, 9.0, activeIdx == 4);

      DrawMenuFooter();
      MenuFrameTick();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    bool bSelect, bBack, bUp, bDown, bLeft, bRight;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, &bLeft, &bRight);

    if (bBack) { MenuBeep(); Sequence_SortByTime(); return; }
    if (bUp)   { MenuBeep(); activeIdx = (activeIdx == 0) ? lineCount - 1 : activeIdx - 1; waitTime = 150; }
    if (bDown) { MenuBeep(); activeIdx = (activeIdx + 1) % lineCount; waitTime = 150; }

    s = Sequence_Active();
    if (!s || eventIdx >= (int)s->events.size()) return;
    EffectEvent &ev = s->events[eventIdx];

    if (bSelect) {
      MenuBeep();
      switch (activeIdx) {
      case 0: { float v; if (PromptForFloat(v, ev.t)) { if (v < 0.0f) v = 0.0f; ev.t = v; } break; }
      case 1: ev.kind = (EffectKind)(((int)ev.kind + 1) % EFX_COUNT); break;
      case 2: { float v; if (PromptForFloat(v, ev.value)) ev.value = v; break; }
      case 3: ev.ramp = !ev.ramp; break;
      case 4: Sequence_DeleteEvent(eventIdx); SetStatusText("Event deleted"); return;
      }
      waitTime = 200;
    }
    // Time (0) and a continuous Value (2) accelerate on hold. The Effect-kind
    // enum (1) and Mode toggle (3) keep a fixed repeat; integer-valued effect
    // kinds (whose step is 1) aren't multiplied so they step one at a time.
    float vstep = EventValueStep(ev.kind);
    bool numericRow = (activeIdx == 0) || (activeIdx == 2 && vstep < 1.0f);
    DWORD adjWait = 100;
    float mult = HoldAccel((bRight || bLeft) && numericRow, holdStart,
                           wasHolding, &adjWait);
    if (activeIdx == 2 && vstep < 1.0f) vstep *= mult;

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 0: ev.t += 0.1f * mult; break;
      case 1: ev.kind = (EffectKind)(((int)ev.kind + 1) % EFX_COUNT); break;
      case 2: ev.value += vstep; break;
      case 3: ev.ramp = !ev.ramp; break;
      }
      waitTime = adjWait;
    }
    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 0: ev.t -= 0.1f * mult; if (ev.t < 0.0f) ev.t = 0.0f; break;
      case 1: ev.kind = (EffectKind)(((int)ev.kind + EFX_COUNT - 1) % EFX_COUNT); break;
      case 2: ev.value -= vstep; break;
      case 3: ev.ramp = !ev.ramp; break;
      }
      waitTime = adjWait;
    }
  }
}
