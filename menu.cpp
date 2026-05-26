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
#include "sequence.h"

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
  // Camera Sequence hotkeys — inert outside Sequence mode.
  g_SeqHotkeyAdd = GetPrivateProfileIntA("Controls", "SequenceAddKey", VK_F6, path);
  g_SeqHotkeyPlay = GetPrivateProfileIntA("Controls", "SequencePlayKey", VK_F7, path);
  g_SeqHotkeyStop = GetPrivateProfileIntA("Controls", "SequenceStopKey", VK_F8, path);
  g_SeqHotkeyNext = GetPrivateProfileIntA("Controls", "SequenceNextKey", VK_F9, path);
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
      // Suppress phone & character wheel while menu is open
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
      std::string followStr = "None";
      if (g_FollowMode == 1)
        followStr = "Player";
      else if (g_FollowMode == 2)
        followStr = "Aimed Entity";
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
      } else if (activeIdx == walkHeightIdx) {
        g_WalkHeight += 0.1f;
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
      } else if (activeIdx == walkHeightIdx) {
        g_WalkHeight -= 0.1f;
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
      // Suppress phone & character wheel while menu is open
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
      // Suppress phone & character wheel while menu is open
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
      // Suppress phone & character wheel while menu is open
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
  const int lineCount = 8;
  int activeIdx = 0;

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      // Suppress phone & character wheel while menu is open
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
      DrawMenuValue("Disable Vehicle Shake", FormatBool(g_DisableVehicleShake),
                    lineWidth, 9.0, 60.0 + 7 * 36.0, 0.0, 9.0, activeIdx == 7);

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
      case 7:
        g_DisableVehicleShake = !g_DisableVehicleShake;
        SetStatusText(g_DisableVehicleShake ? "Vehicle shake disabled"
                                            : "Vehicle shake enabled");
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

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      CONTROLS::DISABLE_CONTROL_ACTION(0, 27, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 172, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 173, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 174, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 175, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 19, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 166, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 167, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 168, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 169, TRUE);

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

    if (bRight) {
      MenuBeep();
      if (activeIdx == 1) {
        int next = (g_ShakePreset + 1) % 5;
        ApplyShakePreset(next);
      } else if (activeIdx == 2) {
        g_ShakeAmp += 0.05f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 3) {
        // Variable step: fine control at low Hz, coarser when already high
        float step = (g_ShakeFreq < 0.5f) ? 0.05f
                   : (g_ShakeFreq < 2.0f) ? 0.1f
                                          : 0.5f;
        g_ShakeFreq += step;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 4) {
        g_ShakeSpeedAmpCoupling += 0.1f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 5) {
        g_ShakeSpeedFreqCoupling += 0.1f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 6) {
        g_ShakeRotWeight += 0.1f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 7) {
        g_ShakePosWeight += 0.1f;
        ClampShake();
        MarkShakeCustom();
      }
      waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 1) {
        int prev = (g_ShakePreset <= 0) ? 4 : g_ShakePreset - 1;
        if (prev > 4) prev = 4;
        ApplyShakePreset(prev);
      } else if (activeIdx == 2) {
        g_ShakeAmp -= 0.05f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 3) {
        // Mirror the right-arrow's adaptive step so increments and
        // decrements feel symmetrical at any current value.
        float step = (g_ShakeFreq <= 0.5f) ? 0.05f
                   : (g_ShakeFreq <= 2.0f) ? 0.1f
                                           : 0.5f;
        g_ShakeFreq -= step;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 4) {
        g_ShakeSpeedAmpCoupling -= 0.1f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 5) {
        g_ShakeSpeedFreqCoupling -= 0.1f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 6) {
        g_ShakeRotWeight -= 0.1f;
        ClampShake();
        MarkShakeCustom();
      } else if (activeIdx == 7) {
        g_ShakePosWeight -= 0.1f;
        ClampShake();
        MarkShakeCustom();
      }
      waitTime = 100;
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
  const int lineCount = 8;
  int activeIdx = 0;

  static const char *lineCaption[lineCount] = {
      "Toggle freecam", "Movement",       "Lens settings",
      "Depth of field", "Camera effects", "Time & weather",
      "Misc settings",  "Exit"};

  DWORD waitTime = 150;
  while (true) {
    DWORD maxTick = GetTickCount() + waitTime;
    do {
      // Suppress phone & character wheel while menu is open
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

      // Title bar
      std::string title = "SIMPLE CAMERA";
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Menu items
      for (int i = 0; i < lineCount; i++) {
        std::string cap = lineCaption[i];
        // Show ON/OFF state for the toggle item using NativeUI formatting
        if (i == 0) {
          DrawMenuValue("Toggle freecam", FormatBool(g_FreeCamActive),
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
        ProcessCameraEffectsMenu();
        break;
      case 5:
        ProcessTimeWeatherMenu();
        break;
      case 6:
        ProcessMiscMenu();
        break;
      case 7:
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
      CONTROLS::DISABLE_CONTROL_ACTION(0, 27, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 172, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 173, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 174, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 175, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 19, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 166, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 167, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 168, TRUE);
      CONTROLS::DISABLE_CONTROL_ACTION(0, 169, TRUE);

      DrawMenuLine("SIMPLE CAMERA", "", lineWidth, 15.0, 18.0, 0.0, 5.0, false,
                   true);
      DrawMenuValue("Free Camera", "Manual flight", lineWidth, 9.0,
                    60.0 + 0 * 36.0, 0.0, 9.0, activeIdx == 0);
      DrawMenuValue("Camera Sequence", "Keyframed animation", lineWidth, 9.0,
                    60.0 + 1 * 36.0, 0.0, 9.0, activeIdx == 1);

      if (g_FreeCamActive && g_CameraMode != 1)
        UpdateFreeCamera();
      UpdateTimeWeather();
      UpdateStatusText();
      UpdateGlobalEffects();
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
static void DisableMenuPhoneControls();
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

// Shared helper — every submenu suppresses the same controls
static void DisableMenuPhoneControls() {
  CONTROLS::DISABLE_CONTROL_ACTION(0, 27, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 172, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 173, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 174, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 175, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 19, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 166, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 167, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 168, TRUE);
  CONTROLS::DISABLE_CONTROL_ACTION(0, 169, TRUE);
}

// ============================================================
//  Top-level dispatch (called by script.cpp on F5)
// ============================================================

void ProcessConfigMenu() {
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
      if (g_CameraMode == 1) Sequence_EnterMode();
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
//  Camera Sequence — main menu
// ============================================================

static std::string FormatTimeSec(float t) {
  char buf[16];
  sprintf_s(buf, "%.2fs", t);
  return std::string(buf);
}

static bool ProcessSequenceMenu() {
  const float lineWidth = 320.0f;
  // 18 rows. "Follow & Entity Lock..." sits between Close Loop and Show
  // Markers so the lock-related controls cluster near the visual toggles.
  // Hide HUD + Hide Player ride above Exit so the user can clean the
  // viewport for composition without leaving Sequence mode.
  //
  // The row count exceeds kMaxVisibleListRows (12), so we render through
  // the same scroll-window pattern that ProcessPoseListMenu / Movement
  // Settings use: scrollOffset persists across the inner draw loop,
  // EnsureRowVisible keeps the cursor in view, vis() / rowTop() drive
  // per-row visibility and Y placement.
  const int lineCount = 18;
  int activeIdx = 0;
  int scrollOffset = 0;

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
        DrawMenuValue("Delete Active Sequence", "Press Enter", lineWidth, 9.0,
                      rowTop(10), 0.0, 9.0, activeIdx == 10);
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
      // Visibility toggles. g_HidePlayer defaults to true the moment we
      // enter Sequence mode (InitFreeCamera flips it); expose the
      // toggle here so users can show the player ped while composing
      // shots with the ped in frame. UpdateGlobalEffects (called every
      // tick) applies the visibility state to the player ped, so the
      // toggle takes effect instantly.
      if (vis(15))
        DrawMenuValue("Hide HUD", FormatBool(g_HideHUD), lineWidth, 9.0,
                      rowTop(15), 0.0, 9.0, activeIdx == 15);
      if (vis(16))
        DrawMenuValue("Hide Player Character", FormatBool(g_HidePlayer),
                      lineWidth, 9.0, rowTop(16), 0.0, 9.0,
                      activeIdx == 16);
      if (vis(17))
        DrawMenuValue("Exit", "Mode picker", lineWidth, 9.0,
                      rowTop(17), 0.0, 9.0, activeIdx == 17);

      // Drive the sequence mode every frame while this menu is open.
      // script.cpp's main loop is blocked at ProcessConfigMenu(), so it
      // can't tick anything — we have to do it ourselves here, just like
      // every Free Camera submenu calls UpdateFreeCamera() in its draw
      // loop. This is what lets the user free-fly the camera while the
      // sequence menu is open AND lets playback animate live.
      if (Sequence_IsInMode()) Sequence_FrameTick();
      UpdateStatusText();
      UpdateGlobalEffects();
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
      case 10: Sequence_DeleteActive(); SetStatusText("Sequence deleted"); break;
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
        g_HideHUD = !g_HideHUD;
        SetStatusText(g_HideHUD ? "HUD Hidden" : "HUD Visible");
        break;
      case 16:
        g_HidePlayer = !g_HidePlayer;
        SetStatusText(g_HidePlayer ? "Player Hidden" : "Player Visible");
        break;
      case 17: return true; // Exit → return to mode picker
      }
      waitTime = 200;
    }

    if (bRight) {
      MenuBeep();
      if (activeIdx == 0) { // cycle sequences
        if (seqIdx < seqCount - 1) Sequence_SetActive(seqIdx + 1);
      } else if (activeIdx == 3 && s) {
        s->loop = !s->loop;
      } else if (activeIdx == 4 && s) {
        s->playbackSpeed += 0.1f;
        if (s->playbackSpeed > 8.0f) s->playbackSpeed = 8.0f;
      } else if (activeIdx == 5) {
        Sequence_SetCurrentTime(Sequence_CurrentTime() + 0.1f);
      }
      waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      if (activeIdx == 0) {
        if (seqIdx > 0) Sequence_SetActive(seqIdx - 1);
      } else if (activeIdx == 3 && s) {
        s->loop = !s->loop;
      } else if (activeIdx == 4 && s) {
        s->playbackSpeed -= 0.1f;
        if (s->playbackSpeed < 0.05f) s->playbackSpeed = 0.05f;
      } else if (activeIdx == 5) {
        Sequence_SetCurrentTime(Sequence_CurrentTime() - 0.1f);
      }
      waitTime = 100;
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
      std::string followStr = "None";
      if (g_FollowMode == 1)      followStr = "Player";
      else if (g_FollowMode == 2) followStr = "Aimed Entity";
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
          // Flags 30 = vehicles(2) | peds(4) | ragdolls(8) | objects(16).
          // The original Movement-menu raycast used 14 (no objects); we
          // widen here so e.g. parked-prop scenery can also be locked.
          int rayHandle = invoke<int>(0x377906D8A31E5586, posX, posY, posZ,
                                      endX, endY, endZ, 30,
                                      PLAYER::PLAYER_PED_ID(), 7);
          int hit = 0, entityHit = 0;
          Vector3 ignore1{}, ignore2{};
          invoke<int>(0x3D87450E15D98694, rayHandle, &hit, &ignore1, &ignore2,
                      &entityHit);
          if (hit && entityHit != 0 && ENTITY::DOES_ENTITY_EXIST(entityHit) &&
              (ENTITY::IS_ENTITY_A_PED(entityHit) ||
               ENTITY::IS_ENTITY_A_VEHICLE(entityHit) ||
               ENTITY::IS_ENTITY_AN_OBJECT(entityHit))) {
            Vector3 entPos = ENTITY::GET_ENTITY_COORDS(entityHit, TRUE);
            GRAPHICS::DRAW_MARKER(0, entPos.x, entPos.y, entPos.z + 1.25f, 0,
                                  0, 0, 0, 0, 0, 0.4f, 0.4f, 0.4f, 255, 255,
                                  255, 200, TRUE, TRUE, 2, FALSE, NULL, NULL,
                                  FALSE);
          }
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
      if (Sequence_IsInMode()) Sequence_FrameTick();
      UpdateStatusText();
      UpdateGlobalEffects();
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
          // user is aiming at. 14 = vehicles|peds|objects flags.
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
          // Flags 30 = vehicles(2) | peds(4) | ragdolls(8) | objects(16).
          // The original Movement-menu raycast used 14 (no objects); we
          // widen here so e.g. parked-prop scenery can also be locked.
          int rayHandle = invoke<int>(0x377906D8A31E5586, posX, posY, posZ,
                                      endX, endY, endZ, 30,
                                      PLAYER::PLAYER_PED_ID(), 7);
          int hit = 0, entityHit = 0;
          Vector3 ignore1{}, ignore2{};
          invoke<int>(0x3D87450E15D98694, rayHandle, &hit, &ignore1, &ignore2,
                      &entityHit);
          if (hit && entityHit != 0 && ENTITY::DOES_ENTITY_EXIST(entityHit) &&
              (ENTITY::IS_ENTITY_A_PED(entityHit) ||
               ENTITY::IS_ENTITY_A_VEHICLE(entityHit) ||
               ENTITY::IS_ENTITY_AN_OBJECT(entityHit))) {
            g_FollowTargetEntity = entityHit;
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

  DWORD waitTime = 150;
  while (true) {
    CameraSequence *s = Sequence_Active();
    int poseCount = s ? (int)s->poses.size() : 0;
    int lineCount = poseCount + 1; // + "Add at current pose"
    if (activeIdx >= lineCount) activeIdx = lineCount - 1;
    if (activeIdx < 0) activeIdx = 0;
    EnsureRowVisible(activeIdx, lineCount, scrollOffset);

    int visEnd = scrollOffset + kMaxVisibleListRows;
    if (visEnd > lineCount) visEnd = lineCount;

    DWORD maxTick = GetTickCount() + waitTime;
    do {
      DisableMenuPhoneControls();
      // Title bar shows logical range / total so the user always knows
      // where they are even when only a slice is rendered.
      char title[64];
      if (lineCount > kMaxVisibleListRows) {
        sprintf_s(title, "POSE KEYFRAMES  [%d-%d / %d]", scrollOffset + 1,
                  visEnd, lineCount);
      } else {
        sprintf_s(title, "POSE KEYFRAMES");
      }
      DrawMenuLine(title, "", lineWidth, 15.0, 18.0, 0.0, 5.0, false, true);

      // Render only the visible slice. visRow is the on-screen row index
      // (0..kMaxVisibleListRows-1); i is the logical list index.
      // Range info (X-Y / total) is in the title bar — that's enough to
      // know how many items are hidden in each direction, so we don't
      // draw separate "more above/below" indicator rows (they crowded
      // the first/last list rows and Unicode arrows rendered as boxes
      // in the GTA Chalet Comprime font).
      for (int i = scrollOffset; i < visEnd; ++i) {
        int visRow = i - scrollOffset;
        float top = 60.0f + visRow * 36.0f;
        if (i < poseCount) {
          const PoseKeyframe &p = s->poses[i];
          char label[64], val[96];
          sprintf_s(label, "KF %d", i);
          sprintf_s(val, "t=%.2fs  fov=%.0f  %s/%s", p.t, p.fov,
                    EaseName(p.ease), PathName(p.path));
          DrawMenuValue(label, val, lineWidth, 9.0, top, 0.0, 9.0,
                        activeIdx == i);
        } else {
          // The trailing "+ Add" sentinel — same row, different content.
          DrawMenuValue("+ Add at current camera pose", "F6", lineWidth, 9.0,
                        top, 0.0, 9.0, activeIdx == i);
        }
      }

      if (Sequence_IsInMode()) Sequence_FrameTick();
      UpdateStatusText();
      UpdateGlobalEffects();
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
      if (activeIdx == poseCount) {
        int newIdx = Sequence_CapturePoseAtCurrentTime();
        if (newIdx >= 0) SetStatusText("Pose captured");
      } else {
        ProcessPoseEditMenu(activeIdx);
      }
      waitTime = 200;
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

      if (Sequence_IsInMode()) Sequence_FrameTick();
      UpdateStatusText();
      UpdateGlobalEffects();
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
    // not nudgeable values.
    if (bRight) {
      MenuBeep();
      if      (activeIdx == IDX_TIME)  { pose.t += 0.1f; if (pose.t < 0.0f) pose.t = 0.0f; }
      else if (activeIdx == IDX_POSX)  pose.posX += 0.5f;
      else if (activeIdx == IDX_POSY)  pose.posY += 0.5f;
      else if (activeIdx == IDX_POSZ)  pose.posZ += 0.5f;
      else if (activeIdx == IDX_PITCH) pose.pitch += 1.0f;
      else if (activeIdx == IDX_YAW)   pose.yaw += 1.0f;
      else if (activeIdx == IDX_ROLL)  pose.roll += 1.0f;
      else if (activeIdx == IDX_FOV)   { pose.fov += 1.0f; if (pose.fov > 130.0f) pose.fov = 130.0f; }
      else if (activeIdx == IDX_EASE)  pose.ease = (EaseType)(((int)pose.ease + 1) % 5);
      else if (activeIdx == IDX_PATH)  pose.path = (PathType)(((int)pose.path + 1) % 2);
      waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      if      (activeIdx == IDX_TIME)  { pose.t -= 0.1f; if (pose.t < 0.0f) pose.t = 0.0f; }
      else if (activeIdx == IDX_POSX)  pose.posX -= 0.5f;
      else if (activeIdx == IDX_POSY)  pose.posY -= 0.5f;
      else if (activeIdx == IDX_POSZ)  pose.posZ -= 0.5f;
      else if (activeIdx == IDX_PITCH) pose.pitch -= 1.0f;
      else if (activeIdx == IDX_YAW)   pose.yaw -= 1.0f;
      else if (activeIdx == IDX_ROLL)  pose.roll -= 1.0f;
      else if (activeIdx == IDX_FOV)   { pose.fov -= 1.0f; if (pose.fov < 5.0f) pose.fov = 5.0f; }
      else if (activeIdx == IDX_EASE)  pose.ease = (EaseType)(((int)pose.ease + 4) % 5);
      else if (activeIdx == IDX_PATH)  pose.path = (PathType)(((int)pose.path + 1) % 2);
      waitTime = 100;
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

      if (Sequence_IsInMode()) Sequence_FrameTick();
      UpdateStatusText();
      UpdateGlobalEffects();
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

      if (Sequence_IsInMode()) Sequence_FrameTick();
      UpdateStatusText();
      UpdateGlobalEffects();
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
    if (bRight) {
      MenuBeep();
      switch (activeIdx) {
      case 0: ev.t += 0.1f; break;
      case 1: ev.kind = (EffectKind)(((int)ev.kind + 1) % EFX_COUNT); break;
      case 2: ev.value += EventValueStep(ev.kind); break;
      case 3: ev.ramp = !ev.ramp; break;
      }
      waitTime = 100;
    }
    if (bLeft) {
      MenuBeep();
      switch (activeIdx) {
      case 0: ev.t -= 0.1f; if (ev.t < 0.0f) ev.t = 0.0f; break;
      case 1: ev.kind = (EffectKind)(((int)ev.kind + EFX_COUNT - 1) % EFX_COUNT); break;
      case 2: ev.value -= EventValueStep(ev.kind); break;
      case 3: ev.ramp = !ev.ramp; break;
      }
      waitTime = 100;
    }
  }
}
