/*
        GTA V Free Camera / Photo Mode Plugin
        In-Game Menu System Implementation

        NativeTrainer-style hierarchical menu with full keyboard + controller
   support. Five categories: Activation, Movement, Lens, Depth of Field, Time &
   Weather.
*/

#include "menu.h"
#include "camera.h"
#include "keyboard.h"

#include <cstdio>
#include <string>

#pragma warning(disable : 4244 4305)

// ============================================================
//  Status Text
// ============================================================

static std::string s_StatusText;
static DWORD s_StatusTextMaxTicks = 0;

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

void LoadSettings() {
  // Build path to INI file next to the ASI
  char path[MAX_PATH];
  HMODULE hMod = NULL;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCSTR)LoadSettings, &hMod);
  GetModuleFileNameA(hMod, path, MAX_PATH);

  // Replace filename with SimpleCamera.ini
  char *last = strrchr(path, '\\');
  if (last)
    strcpy_s(last + 1, MAX_PATH - (last + 1 - path), "SimpleCamera.ini");

  g_MenuKey = GetPrivateProfileIntA("Controls", "MenuKey", VK_F5, path);
}

// ============================================================
//  Menu Toggle
// ============================================================

bool IsMenuTogglePressed() { return IsKeyJustUp(g_MenuKey); }

// ============================================================
//  Drawing Helpers
// ============================================================

static void draw_rect(float x, float y, float w, float h, int r, int g, int b,
                      int a) {
  GRAPHICS::DRAW_RECT((x + (w * 0.5f)), (y + (h * 0.5f)), w, h, r, g, b, a);
}

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
    if (g_FreeCamActive)
      UpdateFreeCamera();
    UpdateStatusText();
    UpdateGlobalEffects();
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
//  Sub-Menu: Movement Settings
// ============================================================

static void ProcessMovementMenu() {
  const float lineWidth = 300.0f;
  static bool s_LockFailed = false;
  int activeIdx = 0;

  DWORD waitTime = 150;
  while (true) {
    // Dynamic line count
    int lineCount = 9; // Speed, Sensitivity, Zoom Speed, Roll Speed, Collision, Lock Height, Acro Mode,
                       // Follow Target, Drone Mode
    if (g_FollowMode == 1) {
      lineCount += 1; // "Rigid Mode"
    } else if (g_FollowMode == 2) {
      lineCount += 1; // "Lock Entity"
      if (g_FollowTargetEntity != 0) {
        lineCount += 2; // "Show Marker" + "Rigid Mode"
      }
    }
    if (g_DroneMode)
      lineCount += 7; // Drone properties (including FOV Smoothing)

    if (activeIdx >= lineCount)
      activeIdx = lineCount - 1;

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DrawMenuLine("MOVEMENT SETTINGS", "", lineWidth, 15.0, 18.0, 0.0, 5.0,
                   false, true);
      int row = 0;
      DrawMenuValue("Camera Speed", FormatFloat(g_CamSpeed), lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;
      DrawMenuValue("Look Sensitivity", FormatFloat(g_CamSensitivity), lineWidth,
                    9.0, 60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;
      DrawMenuValue("Zoom Speed", FormatFloat(g_ZoomSpeed), lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;
      DrawMenuValue("Roll Speed", FormatFloat(g_RollSpeed), lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;
      DrawMenuValue("World Collision", FormatBool(g_CamCollision), lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;
      DrawMenuValue("Lock Altitude", FormatBool(g_LockHeight), lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;

      std::string rotEngineStr = g_RotationEngine ? "Acrobatic" : "Standard";
      DrawMenuValue("Rotation Style", rotEngineStr, lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;

      // Follow Mode
      std::string followStr = "None";
      if (g_FollowMode == 1)
        followStr = "Player";
      else if (g_FollowMode == 2)
        followStr = "Aimed Entity";
      DrawMenuValue("Follow Target", followStr, lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;

      if (g_FollowMode == 1) {
        DrawMenuValue("  - Rigid Mode", FormatBool(g_FollowRigidMode), lineWidth,
                      9.0, 60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;
      } else if (g_FollowMode == 2) {
        std::string entityStr =
            (g_FollowTargetEntity == 0) ? "Click to Lock" : "Locked";

        DrawMenuValue("  - Lock Entity", entityStr, lineWidth, 9.0,
                      60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;

        if (g_FollowTargetEntity != 0) {
          DrawMenuValue("  - Show Marker", FormatBool(g_ShowLockedEntityMarker),
                        lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                        activeIdx == row);
          row++;

          DrawMenuValue("  - Rigid Mode", FormatBool(g_FollowRigidMode),
                        lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                        activeIdx == row);
          row++;
        }

        // Draw hover marker if we are not yet locked onto an entity
        if (g_FollowTargetEntity == 0) {
          float posX, posY, posZ, pitch, yaw, roll;
          GetCameraState(posX, posY, posZ, pitch, yaw, roll);

          float yawRad = yaw * 0.0174532925f;
          float pitchRad = pitch * 0.0174532925f;
          float dirX = -sin(yawRad) * cos(pitchRad);
          float dirY = cos(yawRad) * cos(pitchRad);
          float dirZ = sin(pitchRad);

          float endX = posX + dirX * 1000.0f;
          float endY = posY + dirY * 1000.0f;
          float endZ = posZ + dirZ * 1000.0f;

          // 14 = Vehicles (2) | Peds (4) | Objects (8)
          int handle = invoke<int>(0x377906D8A31E5586, posX, posY, posZ, endX,
                                   endY, endZ, 14, PLAYER::PLAYER_PED_ID(), 7);

          int hit = 0;
          int entityHit = 0;
          Vector3 hitCoords, surfaceNormal;
          invoke<int>(0x3D87450E15D98694, handle, &hit, &hitCoords,
                      &surfaceNormal, &entityHit);

          if (hit && entityHit != 0 && ENTITY::DOES_ENTITY_EXIST(entityHit) &&
              (ENTITY::IS_ENTITY_A_PED(entityHit) ||
               ENTITY::IS_ENTITY_A_VEHICLE(entityHit) ||
               ENTITY::IS_ENTITY_AN_OBJECT(entityHit))) {
            // Draw a white marker above the entity's position
            Vector3 entPos = ENTITY::GET_ENTITY_COORDS(entityHit, TRUE);
            GRAPHICS::DRAW_MARKER(0, entPos.x, entPos.y, entPos.z + 1.25f, 0, 0,
                                  0, 0, 0, 0, 0.4f, 0.4f, 0.4f, 255, 255, 255,
                                  200, TRUE, TRUE, 2, FALSE, NULL, NULL, FALSE);
          }
        }
      }

      std::string moveStyleStr = g_DroneMode ? "Drone" : "Standard";
      DrawMenuValue("Movement Style", moveStyleStr, lineWidth, 9.0,
                    60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
      row++;
      if (g_DroneMode) {
        DrawMenuValue("  - Drag", FormatFloat(g_DroneDrag), lineWidth, 9.0,
                      60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;
        DrawMenuValue("  - Acceleration", FormatFloat(g_DroneAcceleration),
                      lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                      activeIdx == row);
        row++;
        DrawMenuValue("  - Gravity", FormatFloat(g_DroneGravity), lineWidth, 9.0,
                      60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;
        DrawMenuValue("  - Banking", FormatFloat(g_DroneBanking), lineWidth, 9.0,
                      60.0 + row * 36.0, 0.0, 9.0, activeIdx == row);
        row++;
        DrawMenuValue("  - Rot. Smoothing", FormatFloat(g_DroneRotSmoothing),
                      lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                      activeIdx == row);
        row++;
        DrawMenuValue("  - FOV Smoothing", FormatFloat(g_DroneFovSmoothing),
                      lineWidth, 9.0, 60.0 + row * 36.0, 0.0, 9.0,
                      activeIdx == row);
        row++;
      }

      if (g_FreeCamActive)
        UpdateFreeCamera();

      UpdateStatusText();
      UpdateGlobalEffects();
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
    int rotationIdx = 6;
    int followIdx = 7;
 
    int lockIdx = -1;
    int markerIdx = -1;
    int rigidIdx = -1;
    int droneToggleIdx = 8;
 
    if (g_FollowMode == 1) {
      rigidIdx = 8;
      droneToggleIdx = 9;
    } else if (g_FollowMode == 2) {
      lockIdx = 8;
      droneToggleIdx = 9;
      if (g_FollowTargetEntity != 0) {
        markerIdx = 9;
        rigidIdx = 10;
        droneToggleIdx = 11;
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
          float posX, posY, posZ, pitch, yaw, roll;
          GetCameraState(posX, posY, posZ, pitch, yaw, roll);

          float yawRad = yaw * 0.0174532925f;
          float pitchRad = pitch * 0.0174532925f;
          float dirX = -sin(yawRad) * cos(pitchRad);
          float dirY = cos(yawRad) * cos(pitchRad);
          float dirZ = sin(pitchRad);

          float endX = posX + dirX * 1000.0f;
          float endY = posY + dirY * 1000.0f;
          float endZ = posZ + dirZ * 1000.0f;

          // 14 = Vehicles (2) | Peds (4) | Objects (8)
          int handle = invoke<int>(0x377906D8A31E5586, posX, posY, posZ, endX,
                                   endY, endZ, 14, PLAYER::PLAYER_PED_ID(), 7);

          int hit = 0;
          int entityHit = 0;
          Vector3 ignore1, ignore2;
          invoke<int>(0x3D87450E15D98694, handle, &hit, &ignore1, &ignore2,
                      &entityHit); // GET_SHAPE_TEST_RESULT

          if (hit && entityHit != 0 && ENTITY::DOES_ENTITY_EXIST(entityHit) &&
              (ENTITY::IS_ENTITY_A_PED(entityHit) ||
               ENTITY::IS_ENTITY_A_VEHICLE(entityHit) ||
               ENTITY::IS_ENTITY_AN_OBJECT(entityHit))) {
            g_FollowTargetEntity = entityHit;
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

    if (bRight) {
      MenuBeep();
      if (activeIdx == 0) {
        g_CamSpeed += 0.1f;
        if (g_CamSpeed > 50.0f)
          g_CamSpeed = 50.0f;
      } else if (activeIdx == 1) {
        g_CamSensitivity += 0.1f;
        if (g_CamSensitivity > 5.0f)
          g_CamSensitivity = 5.0f;
      } else if (activeIdx == 2) {
        g_ZoomSpeed += 0.1f;
        if (g_ZoomSpeed > 10.0f)
          g_ZoomSpeed = 10.0f;
      } else if (activeIdx == 3) {
        g_RollSpeed += 0.1f;
        if (g_RollSpeed > 10.0f)
          g_RollSpeed = 10.0f;
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
          g_DroneDrag += 0.5f;
          if (g_DroneDrag > 20.0f)
            g_DroneDrag = 20.0f;
          break;
        case 1:
          g_DroneAcceleration += 1.0f;
          if (g_DroneAcceleration > 50.0f)
            g_DroneAcceleration = 50.0f;
          break;
        case 2:
          g_DroneGravity += 0.5f;
          if (g_DroneGravity > 20.0f)
            g_DroneGravity = 20.0f;
          break;
        case 3:
          g_DroneBanking += 1.0f;
          if (g_DroneBanking > 45.0f)
            g_DroneBanking = 45.0f;
          break;
        case 4:
          g_DroneRotSmoothing += 0.5f;
          if (g_DroneRotSmoothing > 20.0f)
            g_DroneRotSmoothing = 20.0f;
          break;
        case 5:
          g_DroneFovSmoothing += 0.5f;
          if (g_DroneFovSmoothing > 20.0f)
            g_DroneFovSmoothing = 20.0f;
          break;
        }
      }
      waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 0) {
        g_CamSpeed -= 0.1f;
        if (g_CamSpeed < 0.1f)
          g_CamSpeed = 0.1f;
      } else if (activeIdx == 1) {
        g_CamSensitivity -= 0.1f;
        if (g_CamSensitivity < 0.1f)
          g_CamSensitivity = 0.1f;
      } else if (activeIdx == 2) {
        g_ZoomSpeed -= 0.1f;
        if (g_ZoomSpeed < 0.1f)
          g_ZoomSpeed = 0.1f;
      } else if (activeIdx == 3) {
        g_RollSpeed -= 0.1f;
        if (g_RollSpeed < 0.1f)
          g_RollSpeed = 0.1f;
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
          g_DroneDrag -= 0.5f;
          if (g_DroneDrag < 0.5f)
            g_DroneDrag = 0.5f;
          break;
        case 1:
          g_DroneAcceleration -= 1.0f;
          if (g_DroneAcceleration < 1.0f)
            g_DroneAcceleration = 1.0f;
          break;
        case 2:
          g_DroneGravity -= 0.5f;
          if (g_DroneGravity < 0.0f)
            g_DroneGravity = 0.0f;
          break;
        case 3:
          g_DroneBanking -= 1.0f;
          if (g_DroneBanking < 0.0f)
            g_DroneBanking = 0.0f;
          break;
        case 4:
          g_DroneRotSmoothing -= 0.5f;
          if (g_DroneRotSmoothing < 1.0f)
            g_DroneRotSmoothing = 1.0f;
          break;
        case 5:
          g_DroneFovSmoothing -= 0.5f;
          if (g_DroneFovSmoothing < 0.0f)
            g_DroneFovSmoothing = 0.0f;
          break;
        }
      }
      waitTime = 100;
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

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DrawMenuLine("LENS SETTINGS", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Lens Zoom (FOV)", FormatFloat(g_CamFOV, 0) + "\xC2\xB0",
                    lineWidth, 9.0, 60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Lens Tilt (Roll)", FormatFloat(g_CamRoll, 0) + "\xC2\xB0",
                    lineWidth, 9.0, 60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);

      if (g_FreeCamActive)
        UpdateFreeCamera();

      UpdateStatusText();
      UpdateGlobalEffects();
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

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        g_CamFOV += 1.0f;
        if (g_CamFOV > 130.0f)
          g_CamFOV = 130.0f;
        break;
      case 1:
        g_CamRoll += 1.0f;
        if (g_CamRoll > 180.0f)
          g_CamRoll -= 360.0f;
        break;
      }
      waitTime = 100;
    }

    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 0:
        g_CamFOV -= 1.0f;
        if (g_CamFOV < 5.0f)
          g_CamFOV = 5.0f;
        break;
      case 1:
        g_CamRoll -= 1.0f;
        if (g_CamRoll < -180.0f)
          g_CamRoll += 360.0f;
        break;
      }
      waitTime = 100;
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

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
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

      if (g_FreeCamActive)
        UpdateFreeCamera();

      UpdateStatusText();
      UpdateGlobalEffects();
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

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 2:
        g_DoFFocusDist += 1.0f;
        if (g_DoFFocusDist > 500.0f)
          g_DoFFocusDist = 500.0f;
        break;
      case 3:
        g_DoFMaxNearInFocus += 0.1f;
        if (g_DoFMaxNearInFocus > 50.0f)
          g_DoFMaxNearInFocus = 50.0f;
        break;
      case 4:
        g_DoFMaxFarInFocus += 0.1f;
        if (g_DoFMaxFarInFocus > 50.0f)
          g_DoFMaxFarInFocus = 50.0f;
        break;
      }
      waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 2:
        g_DoFFocusDist -= 1.0f;
        if (g_DoFFocusDist < 0.5f)
          g_DoFFocusDist = 0.5f;
        break;
      case 3:
        g_DoFMaxNearInFocus -= 0.1f;
        if (g_DoFMaxNearInFocus < 0.0f)
          g_DoFMaxNearInFocus = 0.0f;
        break;
      case 4:
        g_DoFMaxFarInFocus -= 0.1f;
        if (g_DoFMaxFarInFocus < 0.0f)
          g_DoFMaxFarInFocus = 0.0f;
        break;
      }
      waitTime = 100;
    }
  }
}

// ============================================================
//  Sub-Menu: Time & Weather
// ============================================================

static void ProcessTimeWeatherMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 8;
  int activeIdx = 0;

  // Hold-to-accelerate tracking for time-of-day left/right
  DWORD timeHoldStart = 0;   // GetTickCount() when the button was first held
  bool  wasHoldingTime = false;  // was left/right held on previous iteration?

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
      DrawMenuValue("Freeze Entire World", FormatBool(g_FreezeWorld), lineWidth, 9.0,
                    60.0 + 7 * 36.0, 0.0, 9.0, activeIdx == 7);

      if (g_FreeCamActive)
        UpdateFreeCamera();

      UpdateTimeWeather();
      UpdateStatusText();
      UpdateGlobalEffects();
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
            TIME::SET_CLOCK_TIME(g_TimeHour, g_TimeMinute, 0);
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
        if (!g_FreezeWorld) {
          GAMEPLAY::SET_TIME_SCALE(1.0f);
        }
        SetStatusText(g_FreezeWorld ? "World frozen" : "World unfrozen");
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

    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 1: {
        int totalMinutes = g_TimeHour * 60 + g_TimeMinute + timeStep;
        totalMinutes %= (24 * 60);
        g_TimeHour   = totalMinutes / 60;
        g_TimeMinute = totalMinutes % 60;
        if (!g_TimePaused) {
          TIME::SET_CLOCK_TIME(g_TimeHour, g_TimeMinute, 0);
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
        g_WeatherBlend += 0.05f;
        if (g_WeatherBlend > 1.0f)
          g_WeatherBlend = 1.0f;
        break;
      }
      if (activeIdx != 1)
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
          TIME::SET_CLOCK_TIME(g_TimeHour, g_TimeMinute, 0);
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
        g_WeatherBlend -= 0.05f;
        if (g_WeatherBlend < 0.0f)
          g_WeatherBlend = 0.0f;
        break;
      }
      if (activeIdx != 1)
        waitTime = 100;
    }
  }
}

// ============================================================
//  Sub-Menu: Misc Settings
// ============================================================

static void ProcessMiscMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 7;
  int activeIdx = 0;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DrawMenuLine("MISC SETTINGS", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Hide Game HUD", FormatBool(g_HideHUD), lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Hide Player Character", FormatBool(g_HidePlayer), lineWidth, 9.0,
                    60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);
      DrawMenuValue("Move Player with Camera", FormatBool(g_MovePlayerWithCamera),
                    lineWidth, 9.0, 60.0 + 2 * 36.0, 0.0, 9.0, activeIdx == 2);
      DrawMenuValue("Save Position on Exit", FormatBool(g_RememberCamPosition),
                    lineWidth, 9.0, 60.0 + 3 * 36.0, 0.0, 9.0, activeIdx == 3);
      DrawMenuValue("Show Info Overlay", FormatBool(g_ShowInfoOverlay), lineWidth,
                    9.0, 60.0 + 4 * 36.0, 0.0, 9.0, activeIdx == 4);
      DrawMenuValue("Lock Camera Position", FormatBool(g_LockCamera), lineWidth, 9.0,
                    60.0 + 5 * 36.0, 0.0, 9.0, activeIdx == 5);
      DrawMenuValue("Allow Player to Move", FormatBool(g_EnablePlayerMovement),
                    lineWidth, 9.0, 60.0 + 6 * 36.0, 0.0, 9.0, activeIdx == 6);

      if (g_FreeCamActive)
        UpdateFreeCamera();

      UpdateStatusText();
      UpdateGlobalEffects();
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
      switch (activeIdx) {
      case 0:
        g_HideHUD = !g_HideHUD;
        SetStatusText(g_HideHUD ? "HUD Hidden" : "HUD Visible");
        break;
      case 1:
        g_HidePlayer = !g_HidePlayer;
        SetStatusText(g_HidePlayer ? "Player Hidden" : "Player Visible");
        break;
      case 2:
        if (!g_FreeCamActive) {
          SetStatusText("This option is exclusively for Free Camera mode.");
          break;
        }
        g_MovePlayerWithCamera = !g_MovePlayerWithCamera;
        if (g_MovePlayerWithCamera && g_FollowMode == 1) {
          g_FollowMode = 0; // Prevent getting stuck in recursive lock
        }
        SetStatusText(g_MovePlayerWithCamera ? "Player follows camera"
                                             : "Player stay fixed");
        break;
      case 3:
        if (!g_FreeCamActive) {
          SetStatusText("This option is exclusively for Free Camera mode.");
          break;
        }
        g_RememberCamPosition = !g_RememberCamPosition;
        SetStatusText(g_RememberCamPosition ? "Camera position remembered"
                                            : "Camera resets on toggle");
        break;
      case 4:
        g_ShowInfoOverlay = !g_ShowInfoOverlay;
        SetStatusText(g_ShowInfoOverlay ? "Info overlay shown"
                                        : "Info overlay hidden");
        break;
      case 5:
        if (!g_FreeCamActive) {
          SetStatusText("This option is exclusively for Free Camera mode.");
          break;
        }
        g_LockCamera = !g_LockCamera;
        SetStatusText(g_LockCamera ? "Camera locked" : "Camera unlocked");
        break;
      case 6:
        if (!g_FreeCamActive) {
          SetStatusText("This option is exclusively for Free Camera mode.");
          break;
        }
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
      }
      waitTime = 200;
    }
  }
}

// ============================================================
//  Main Configuration Menu
// ============================================================

void ProcessConfigMenu() {
  const float lineWidth = 300.0f;
  const int lineCount = 6;
  int activeIdx = 0;

  static const char *lineCaption[lineCount] = {
      "TOGGLE FREECAM", "MOVEMENT",       "LENS SETTINGS",
      "DEPTH OF FIELD", "TIME & WEATHER", "MISC SETTINGS"};

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      // Title bar
      std::string title = "SIMPLE CAMERA";
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Menu items
      for (int i = 0; i < lineCount; i++) {
        std::string cap = lineCaption[i];
        // Show ON/OFF state for the toggle item using NativeUI formatting
        if (i == 0) {
          DrawMenuValue("TOGGLE FREECAM", FormatBool(g_FreeCamActive),
                        lineWidth, i != activeIdx ? 9.0 : 11.0,
                        (i != activeIdx ? 60.0 : 56.0) + i * 36.0, 0.0,
                        i != activeIdx ? 9.0 : 7.0, i == activeIdx);
        } else {
          // Normal Submenus
          if (i != activeIdx)
            DrawMenuLine(cap, "", lineWidth, 9.0, 60.0 + i * 36.0, 0.0, 9.0,
                         false, false);
          else
            DrawMenuLine(cap, "", lineWidth + 1.0, 11.0, 56.0 + i * 36.0, 0.0,
                         7.0, true, false);
        }
      }

      // Keep camera/features alive while browsing the menu
      if (g_FreeCamActive)
        UpdateFreeCamera();

      UpdateTimeWeather();
      UpdateStatusText();
      UpdateGlobalEffects();
      WAIT(0);
    } while (GetTickCount() < maxTick);
    waitTime = 0;

    // Process buttons
    bool bSelect, bBack, bUp, bDown;
    GetMenuButtons(&bSelect, &bBack, &bUp, &bDown, NULL, NULL);

    if (bSelect) {
      MenuBeep();
      switch (activeIdx) {
      case 0: // Toggle freecam
        if (g_FreeCamActive) {
          DestroyFreeCamera();
          SetStatusText("Simple Camera OFF");
        } else {
          InitFreeCamera();
          SetStatusText("Simple Camera ON");
        }
        waitTime = 200;
        break;
      case 1:
        ProcessMovementMenu();
        break;
      case 2:
        ProcessLensMenu();
        break;
      case 3:
        ProcessDoFMenu();
        break;
      case 4:
        ProcessTimeWeatherMenu();
        break;
      case 5:
        ProcessMiscMenu();
        break;
      }
      waitTime = 200;
    } else if (bBack || IsMenuTogglePressed()) {
      MenuBeep();
      break;
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
