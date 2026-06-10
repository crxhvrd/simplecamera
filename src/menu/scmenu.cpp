/*
        Simple Camera — new GUI menu implementation. See scmenu.h.

        Builds the Free Camera menu tree on the gtam::NativeMenu framework using
        the Spotlight (dark + cyan) theme, binding every row to the existing
        camera.h globals and actions. Nothing here duplicates camera logic — it's
        purely a new front-end over the same state the classic menu drives.
*/

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "scmenu.h"
#include "camera.h"
#include "fx_capture.h" // FxCapture_AddonPresent
#include "menu.h"     // SetStatusText, SaveSettings, Reset, render globals + ProcessRenderToImages
#include "sequence.h" // Camera Sequence data model + API

#include "NativeMenu.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ============================================================
//  Menu objects (host-owned, alive for the process lifetime)
// ============================================================

static gtam::MenuController g_Ctrl;
// The single unified root. A "Camera Mode" row at the top switches Off / Free
// Camera / Camera Sequence; the rest of the page is rebuilt to match the mode.
static gtam::Menu g_Root("SIMPLE CAMERA", "Free Camera");
static gtam::Menu g_Movement("SIMPLE CAMERA", "Movement");
static gtam::Menu g_Drone("SIMPLE CAMERA", "Drone Settings");
static gtam::Menu g_Follow("SIMPLE CAMERA", "Follow Target");
static gtam::Menu g_Lens("SIMPLE CAMERA", "Lens");
static gtam::Menu g_DoF("SIMPLE CAMERA", "Depth of Field");
static gtam::Menu g_Effects("SIMPLE CAMERA", "Camera Effects");
static gtam::Menu g_World("SIMPLE CAMERA", "World & Scene");
static gtam::Menu g_Time("SIMPLE CAMERA", "Time & Weather");
static gtam::Menu g_AutoDrive("SIMPLE CAMERA", "Auto Drive");
static gtam::Menu g_Misc("SIMPLE CAMERA", "Misc");
static gtam::Menu g_Appearance("SIMPLE CAMERA", "Appearance");
static gtam::Menu g_ApLayout("SIMPLE CAMERA", "Menu Layout");
static gtam::Menu g_ApColors("SIMPLE CAMERA", "Menu Colours");
static gtam::Menu g_ApMarkers("SIMPLE CAMERA", "Sequence Markers");

// Menu appearance state (persisted via Load/SaveSettings; see scmenu.h).
// Defaults mirror the Spotlight theme so the out-of-box look is unchanged.
float g_MenuPosX = 0.025f, g_MenuPosY = 0.07f, g_MenuScale = 1.0f;
int g_MenuAccentR = 60, g_MenuAccentG = 200, g_MenuAccentB = 220;
int g_MenuBgR = 17, g_MenuBgG = 19, g_MenuBgB = 24;
int g_MenuTextR = 205, g_MenuTextG = 210, g_MenuTextB = 218;
int g_MenuSelR = 30, g_MenuSelG = 80, g_MenuSelB = 95;
int g_MenuSelTextR = 240, g_MenuSelTextG = 245, g_MenuSelTextB = 250;

// ---- Camera Sequence menus ----
static gtam::Menu g_SeqPoses("CAMERA SEQUENCE", "Pose Keyframes");
static gtam::Menu g_SeqPoseEdit("CAMERA SEQUENCE", "Keyframe");
static gtam::Menu g_SeqEvents("CAMERA SEQUENCE", "Effect Events");
static gtam::Menu g_SeqEventEdit("CAMERA SEQUENCE", "Effect Event");
static gtam::Menu g_SeqFollow("CAMERA SEQUENCE", "Follow & Entity Lock");
static gtam::Menu g_SeqList("CAMERA SEQUENCE", "Sequences");
static gtam::Menu g_SeqPlayback("CAMERA SEQUENCE", "Playback");
static gtam::Menu g_SeqRender("CAMERA SEQUENCE", "Render to Images");

// Sequence editing mirrors. The framework binds raw pointers, but pose/event
// vectors can reallocate (capture/add/sort), so editors bind to these stable
// mirrors and we push them to the live element by index each frame.
static float s_scrub = 0.0f;     // root scrub time (synced to playback)
static float s_totalDur = 0.0f;  // pose-list "Total Duration" (scales times)
static int s_editPose = -1;
static int s_editEvent = -1;
static struct { float t, posX, posY, posZ, pitch, yaw, roll, fov; int easeI, pathI; } s_pm;
static struct { float t, value; int kindI; bool ramp; } s_em;
static bool s_seqPopRequested = false; // deferred Back() (delete-from-editor)
static DWORD s_seqDelArmed = 0;        // 2-press confirm for Delete Active
static gtam::MenuItem *s_pEventValue = nullptr;

// Render menu: the FPS / blur settings are non-linear preset cycles, so they
// bind to mirror indices synced from the live (float/int) globals on entry.
static const float kRenderFps[] = {24, 25, 30, 48, 50, 60, 120, 240};
static const int kRenderFpsCount = 8;
static const int kRenderBlur[] = {1, 2, 4, 8, 16, 32, 64};
static const int kRenderBlurCount = 7;
static int s_fpsIdx = 5;  // -> 60
static int s_blurIdx = 0; // -> 1 (Off)

// Unified-root "Camera Mode" mirror: 0 = Off, 1 = Free Camera, 2 = Sequence.
// Synced from the live camera state each frame; changing it applies the switch.
static int s_mode = 0;

static bool g_Built = false;

// ============================================================
//  Helpers
// ============================================================

// Fire a ray from the camera and return the entity under the crosshair (0 if
// none). Mirrors the classic menu's RaycastEntityFromCamera so "Lock Aimed
// Entity" works the same way.
static int RaycastEntityFromCamera() {
  float px, py, pz, pitch, yaw, roll;
  GetCameraState(px, py, pz, pitch, yaw, roll);
  float yawRad = yaw * 0.0174532925f;
  float pitchRad = pitch * 0.0174532925f;
  float dx = -sinf(yawRad) * cosf(pitchRad);
  float dy = cosf(yawRad) * cosf(pitchRad);
  float dz = sinf(pitchRad);
  float ex = px + dx * 1000.0f, ey = py + dy * 1000.0f, ez = pz + dz * 1000.0f;
  int ray = invoke<int>(0x377906D8A31E5586, px, py, pz, ex, ey, ez, 30,
                        PLAYER::PLAYER_PED_ID(), 7);
  int hit = 0, ent = 0;
  Vector3 a{}, b{};
  invoke<int>(0x3D87450E15D98694, ray, &hit, &a, &b, &ent);
  if (hit && ent != 0 && ENTITY::DOES_ENTITY_EXIST(ent) &&
      (ENTITY::IS_ENTITY_A_PED(ent) || ENTITY::IS_ENTITY_A_VEHICLE(ent) ||
       ENTITY::IS_ENTITY_AN_OBJECT(ent)))
    return ent;
  return 0;
}

// Weather / driving-style / effect option lists, built once from the tables.
static std::vector<std::string> g_WeatherOpts;
static std::vector<std::string> g_StyleOpts;
static std::vector<std::string> g_EffectOpts;

static void BuildOptionLists() {
  g_WeatherOpts.clear();
  for (int i = 0; i < g_WeatherCount; ++i)
    g_WeatherOpts.push_back(g_WeatherNames[i]);
  g_StyleOpts.clear();
  for (int i = 0; i < g_AutoDriveStyleCount; ++i)
    g_StyleOpts.push_back(g_AutoDriveStyleNames[i]);
}

// ============================================================
//  Tree construction
// ============================================================

// Build the menu theme from the Spotlight preset + Simple Camera's tweaks +
// the user's accent colour. Called at startup and whenever an accent slider
// changes, so the whole menu recolours live from one RGB.
static void ApplyMenuTheme() {
  gtam::MenuTheme t = gtam::MenuTheme::Spotlight();
  t.maxVisibleRows = 12;
  t.valuePill = false; // no translucent box behind adjustable values
  // Clean sans-serif title (GTA font 0) instead of the cursive House Script.
  t.titleFont = 0;
  t.titleScale = 0.50f;
  t.titleHeightPx = 54.0f;
  gtam::Color ac(g_MenuAccentR, g_MenuAccentG, g_MenuAccentB, 255);
  gtam::Color bg(g_MenuBgR, g_MenuBgG, g_MenuBgB);
  gtam::Color tx(g_MenuTextR, g_MenuTextG, g_MenuTextB, 255);

  // Accent chrome (left bar, selected outline, scrollbar, checkbox fill).
  t.accent = ac;
  t.rowSelectedOutline = ac;
  t.scrollThumb = ac;
  t.checkboxFill = ac;

  // Background family — recolour every panel surface, preserving each one's
  // original translucency so the layered look survives.
  t.titleBg = gtam::Color(bg.r, bg.g, bg.b, 245);
  t.rowBg = gtam::Color(bg.r, bg.g, bg.b, 230);
  t.subtitleBg = gtam::Color(bg.r, bg.g, bg.b, 245);
  t.descBg = gtam::Color(bg.r, bg.g, bg.b, 240);
  t.footerBg = gtam::Color(bg.r, bg.g, bg.b, 240);

  // Text family — captions/values/title/subtitle/description; dimmed variants
  // for separators, disabled rows and the footer.
  t.titleText = tx;
  t.rowText = tx;
  t.subtitleText = tx;
  t.descText = tx;
  t.separatorText = gtam::Color((int)(tx.r * 0.62f), (int)(tx.g * 0.62f), (int)(tx.b * 0.62f), 255);
  t.disabledText = gtam::Color((int)(tx.r * 0.50f), (int)(tx.g * 0.50f), (int)(tx.b * 0.50f), 255);
  t.footerText = gtam::Color((int)(tx.r * 0.78f), (int)(tx.g * 0.78f), (int)(tx.b * 0.78f), 255);

  // Selected row.
  t.rowSelectedBg = gtam::Color(g_MenuSelR, g_MenuSelG, g_MenuSelB, 235);
  t.rowSelectedText = gtam::Color(g_MenuSelTextR, g_MenuSelTextG, g_MenuSelTextB, 255);

  g_Ctrl.SetTheme(t);
}

static void BuildTree() {
  BuildOptionLists();

  ApplyMenuTheme();
  g_Ctrl.SetPosition(g_MenuPosX, g_MenuPosY);
  g_Ctrl.SetScale(g_MenuScale);

  g_Ctrl.SetFooterText("");

  // The unified root's rows are built per-mode in RebuildRoot(); the leaf
  // submenus below are built once here.

  // ---- Movement ----
  g_Movement.AddFloat("Camera Speed", &g_CamSpeed, 0.1f, 50.0f, 0.1f, 1, nullptr,
                      "How fast the camera flies through the world. Higher moves quicker.");
  g_Movement.AddFloat("Look Sensitivity", &g_CamSensitivity, 0.1f, 5.0f, 0.1f, 1, nullptr,
                      "How fast the camera turns when you move the mouse or stick.");
  g_Movement.AddFloat("Zoom Speed", &g_ZoomSpeed, 0.1f, 10.0f, 0.1f, 1, nullptr,
                      "How quickly the lens zooms (changes FOV) when you scroll.");
  g_Movement.AddFloat("Roll Speed", &g_RollSpeed, 0.1f, 10.0f, 0.1f, 1, nullptr,
                      "How fast the camera tilts side-to-side (Dutch angle).");
  g_Movement.AddToggle("World Collision", &g_CamCollision, nullptr,
                       "Camera collides with the world.");
  g_Movement.AddToggle("Lock Altitude", &g_LockHeight, nullptr,
                       "Freeze the Z axis while flying.");
  g_Movement.AddToggle("Walk Mode", &g_WalkMode, nullptr,
                       "Follow the terrain at a fixed eye height.");
  g_Movement.AddFloat("Walk Height (m)", &g_WalkHeight, 0.1f, 50.0f, 0.1f, 2,
                      nullptr, "Eye height above ground in Walk Mode.");
  g_Movement.AddToggle("Acrobatic Rotation", &g_RotationEngine, nullptr,
                       "Quaternion engine: full 360 roll, no gimbal lock.");
  g_Movement.AddSubmenu("Drone Settings", &g_Drone,
                        "Momentum-based flight tuning.");
  g_Movement.AddSubmenu("Follow Target", &g_Follow,
                        "Track the player or an aimed entity.");

  // ---- Drone ----
  g_Drone.AddToggle("Drone Mode", &g_DroneMode, nullptr,
                    "Momentum/inertia flight instead of direct movement.");
  g_Drone.AddFloat("Drag", &g_DroneDrag, 0.0f, 20.0f, 0.5f, 1, nullptr,
                   "Air resistance. Higher stops the camera sooner after you let go.");
  g_Drone.AddFloat("Acceleration", &g_DroneAcceleration, 1.0f, 50.0f, 1.0f, 1, nullptr,
                   "How hard the camera thrusts. Higher reaches top speed faster.");
  g_Drone.AddFloat("Gravity", &g_DroneGravity, 0.0f, 20.0f, 0.5f, 1, nullptr,
                   "Constant downward pull when not thrusting. 0 = weightless / floaty.");
  g_Drone.AddFloat("Banking", &g_DroneBanking, 0.0f, 45.0f, 1.0f, 1, nullptr,
                   "How much the camera auto-rolls into turns, in degrees. 0 = stays level.");
  g_Drone.AddFloat("Rotation Smoothing", &g_DroneRotSmoothing, 0.0f, 20.0f, 0.5f, 1, nullptr,
                   "Turn responsiveness. Higher = snappier, lower = heavier and laggier.");
  g_Drone.AddFloat("FOV Smoothing", &g_DroneFovSmoothing, 0.0f, 20.0f, 0.5f, 1, nullptr,
                   "How smoothly the zoom eases in drone mode. Higher = softer.");

  // ---- Follow ----
  g_Follow.AddList("Follow Mode", &g_FollowMode, {"None", "Player", "Aimed Entity"},
                   [](int m) { if (m != 2) g_FollowTargetEntity = 0; },
                   "What the camera tracks.");
  g_Follow.AddToggle("Rigid Mode", &g_FollowRigidMode, nullptr,
                     "Inherit the target's rotation, not just position.");
  g_Follow.AddToggle("Show Locked Marker", &g_ShowLockedEntityMarker, nullptr,
                     "Draw a marker on the entity the camera is locked to.");
  g_Follow.AddButton("Lock Aimed Entity", [] {
    int e = RaycastEntityFromCamera();
    if (e) {
      g_FollowTargetEntity = e;
      g_FollowMode = 2;
      SetStatusText("Locked onto entity");
    } else {
      SetStatusText("No entity found. Aim closer.");
    }
  }, "Point the camera at a ped/vehicle/object and lock onto it.");
  g_Follow.AddButton("Unlock Entity", [] {
    g_FollowTargetEntity = 0;
    SetStatusText("Entity unlocked");
  }, "Release the currently locked entity.");

  // ---- Lens ----
  g_Lens.AddFloat("Field of View", &g_CamFOV, 5.0f, 130.0f, 1.0f, 0, nullptr,
                  "Lens zoom in degrees. Lower = more zoom.");
  g_Lens.AddFloat("Lens Roll", &g_CamRoll, -180.0f, 180.0f, 1.0f, 0, nullptr,
                  "Dutch-angle tilt in degrees.");

  // ---- Depth of Field ----
  g_DoF.AddToggle("Depth of Field", &g_DoFEnabled, nullptr,
                  "Blur everything except what's in focus (cinematic bokeh).");
  g_DoF.AddToggle("Auto-Focus", &g_DoFAutofocus, nullptr,
                  "Focus on whatever is at the centre of the screen.");
  g_DoF.AddFloat("Manual Focus Dist.", &g_DoFFocusDist, 0.5f, 500.0f, 1.0f, 1, nullptr,
                 "Distance (m) the lens focuses at when Auto-Focus is off.");
  g_DoF.AddFloat("Near Focus Range", &g_DoFMaxNearInFocus, 0.0f, 50.0f, 0.1f, 1, nullptr,
                 "How far in FRONT of the focus point stays sharp (metres).");
  g_DoF.AddFloat("Far Focus Range", &g_DoFMaxFarInFocus, 0.0f, 50.0f, 0.1f, 1, nullptr,
                 "How far BEHIND the focus point stays sharp (metres).");

  // ---- Camera Effects (shake) ----
  // Editing any numeric shake row marks the preset Custom (index 5), matching
  // the classic menu's MarkShakeCustom behaviour.
  auto markCustom = [](float) { g_ShakePreset = 5; };
  g_Effects.AddToggle("Enabled", &g_ShakeEnabled, nullptr,
                      "Turn the procedural handheld camera shake on or off.");
  g_Effects.AddList("Preset", &g_ShakePreset,
                    {"Off", "Subtle", "Handheld", "Vehicle", "Earthquake", "Custom"},
                    [](int idx) { if (idx >= 0 && idx < 5) ApplyShakePreset(idx); },
                    "Pick a ready-made feel; editing a value below switches to Custom.");
  g_Effects.AddFloat("Base Amplitude", &g_ShakeAmp, 0.0f, 3.0f, 0.05f, 2, markCustom,
                     "Overall strength of the shake. 0 = none.");
  g_Effects.AddFloat("Base Frequency (Hz)", &g_ShakeFreq, 0.05f, 20.0f, 0.1f, 2, markCustom,
                     "How fast the shake oscillates. Higher = jittery, lower = slow sway.");
  g_Effects.AddFloat("Speed -> Amplitude", &g_ShakeSpeedAmpCoupling, 0.0f, 2.0f, 0.1f, 2, markCustom,
                     "How much faster camera movement increases the shake strength.");
  g_Effects.AddFloat("Speed -> Frequency", &g_ShakeSpeedFreqCoupling, 0.0f, 2.0f, 0.1f, 2, markCustom,
                     "How much faster camera movement speeds up the shake.");
  g_Effects.AddFloat("Rotation Weight", &g_ShakeRotWeight, 0.0f, 2.0f, 0.1f, 2, markCustom,
                     "How much the shake rotates the camera (vs. shifting it).");
  g_Effects.AddFloat("Position Weight", &g_ShakePosWeight, 0.0f, 2.0f, 0.1f, 2, markCustom,
                     "How much the shake shifts the camera (vs. rotating it).");
  g_Effects.AddToggle("Stop When Still", &g_ShakeStopWhenStill, nullptr,
                      "Fade the shake out when the camera isn't moving.");
  g_Effects.AddButton("Randomize Pattern", [] {
    RandomizeShakePattern();
    SetStatusText("Shake pattern randomized");
  }, "Re-roll the random noise so the shake feels different.");

  // ---- World & Scene ----
  g_World.AddSubmenu("Time & Weather", &g_Time, "Clock, time-lapse, weather.");
  g_World.AddSubmenu("Auto Drive", &g_AutoDrive,
                     "Let the AI drive your car while you film.");
  g_World.AddToggle("Hide Game HUD", &g_HideHUD, nullptr,
                    "Hide the game's HUD (radar, health, weapon) for clean shots.");
  g_World.AddToggle("Hide Player Character", &g_HidePlayer, nullptr,
                    "Make your character invisible while filming.");
  g_World.AddToggle("Disable Vehicle Shake", &g_DisableVehicleShake, nullptr,
                    "Removes the engine-induced body jitter vehicles have while idling "
                    "and driving, so cars sit dead still for clean shots.");
  g_World.AddToggle("Show Info Overlay", &g_ShowInfoOverlay, nullptr,
                    "Show a debug overlay with the camera's position and FOV.");
  g_World.AddSeparator("");
  g_World.AddButton("Save Settings to INI", [] {
    SaveSettings();
    SetStatusText("Settings saved to SimpleCamera.ini");
  }, "Write all current settings to SimpleCamera.ini so they persist.");
  g_World.AddButton("Reset to Defaults", [] {
    ResetSettingsToDefaults();
    // Re-apply the appearance globals to the live menu.
    ApplyMenuTheme();
    g_Ctrl.SetPosition(g_MenuPosX, g_MenuPosY);
    g_Ctrl.SetScale(g_MenuScale);
    SetStatusText("Settings reset to defaults");
  }, "Restore every tunable to its factory value.");

  // ---- Time & Weather ----
  g_Time.AddToggle("Pause Time of Day", &g_TimePaused, nullptr,
                   "Freeze the in-game clock so the sun and lighting stop moving.");
  g_Time.AddInt("Hour", &g_TimeHour, 0, 23, 1, [](int) {
    if (!g_TimePaused) SetClockTime(g_TimeHour, g_TimeMinute, 0);
  }, "Set the hour of the in-game clock (0-23).");
  g_Time.AddInt("Minute", &g_TimeMinute, 0, 59, 1, [](int) {
    if (!g_TimePaused) SetClockTime(g_TimeHour, g_TimeMinute, 0);
  }, "Set the minute of the in-game clock (0-59).");
  g_Time.AddList("Time-lapse Speed", &g_TimelapseMode,
                 {"Off", "Slow", "Medium", "Fast"}, nullptr,
                 "Auto-advance the clock for time-lapse shots. Off = normal speed.");
  g_Time.AddList("Primary Weather", &g_Weather1Index, g_WeatherOpts, nullptr,
                 "The main weather type, applied when you press Apply Weather.");
  g_Time.AddList("Secondary Weather", &g_Weather2Index, g_WeatherOpts, nullptr,
                 "A second weather to blend toward (use the Weather Blend slider).");
  g_Time.AddFloat("Weather Blend", &g_WeatherBlend, 0.0f, 1.0f, 0.05f, 2, nullptr,
                  "Mix between primary and secondary weather. 0% = primary only.");
  g_Time.AddButton("Apply Weather", [] {
    if (g_WeatherBlend <= 0.0f) {
      g_BlendWeatherActive = false;
      GAMEPLAY::SET_WEATHER_TYPE_NOW_PERSIST((char *)g_WeatherNames[g_Weather1Index]);
      SetStatusText(std::string("Weather: ") + g_WeatherNames[g_Weather1Index]);
    } else {
      g_BlendWeatherActive = true;
      SetStatusText(std::string("Blended: ") + g_WeatherNames[g_Weather1Index] +
                    " | " + g_WeatherNames[g_Weather2Index]);
    }
  }, "Commit the chosen weather (or blend) to the world.");
  g_Time.AddToggle("Pause Game (full freeze)", &g_FreezeWorld, nullptr,
                   "Pause the entire world - everything halts (overrides Slow Motion).");
  g_Time.AddToggle("Freeze All Entities", &g_FreezeEntities, nullptr,
                   "Freeze peds and vehicles in place while the camera stays live.");
  g_Time.AddFloat("Slow Motion", &g_WorldTimeScale, 0.01f, 1.0f, 0.01f, 2, nullptr,
                  "World time scale for slow-motion. 1.00 = real time, lower = slower.");

  // ---- Auto Drive ----
  g_AutoDrive.AddToggle("Enabled", &g_AutoDriveEnabled, [](bool on) {
    if (!on) AutoDrive_Stop();
  }, "AI drives your current land vehicle.");
  g_AutoDrive.AddList("Destination", &g_AutoDriveMode,
                      {"Go To Waypoint", "Drive Anywhere"}, nullptr,
                      "Drive to your map waypoint, or wander the roads freely.");
  g_AutoDrive.AddFloat("Speed (m/s)", &g_AutoDriveSpeed, 1.0f, 100.0f, 1.0f, 0, nullptr,
                       "Target driving speed for the AI, in metres per second.");
  g_AutoDrive.AddList("Driving Style", &g_AutoDriveStyleIndex, g_StyleOpts, nullptr,
                      "How the AI drives - cautious, normal, or rushed/aggressive.");

  // ---- Misc ----
  g_Misc.AddToggle("Move Player with Camera", &g_MovePlayerWithCamera, [](bool on) {
    if (on && g_FollowMode == 1) g_FollowMode = 0;
  }, "Drag your character along underneath the camera as it flies.");
  g_Misc.AddToggle("Save Position on Exit", &g_RememberCamPosition, nullptr,
                   "Remember the camera's spot so it reopens where you left it.");
  g_Misc.AddToggle("Lock Camera Position", &g_LockCamera, nullptr,
                   "Freeze the camera completely - all movement and rotation "
                   "input is ignored until you turn this off.");
  g_Misc.AddToggle("Allow Player to Move", &g_EnablePlayerMovement, [](bool on) {
    Ped ped = PLAYER::PLAYER_PED_ID();
    ENTITY::FREEZE_ENTITY_POSITION(ped, on ? FALSE : TRUE);
    ENTITY::SET_ENTITY_COLLISION(ped, on ? TRUE : FALSE, FALSE);
  }, "Let your character walk around while the camera is active.");
  g_Misc.AddButton("Snap Camera to Player", [] {
    SnapCameraToPlayer();
    SetStatusText("Camera snapped to player");
  }, "Jump the camera back to your character's position.");
  g_Misc.AddButton("Level Horizon (reset roll)", [] {
    LevelCameraHorizon();
    SetStatusText("Horizon leveled (roll reset)");
  }, "Reset any side-tilt (Dutch angle) back to level.");

  // ---- Appearance: hub of focused sub-pages ----
  g_Appearance.AddSubmenu("Menu Layout", &g_ApLayout,
                          "Where the menu sits and how big it is.");
  g_Appearance.AddSubmenu("Menu Colours", &g_ApColors,
                          "Accent, background, text and selected-row colours.");
  g_Appearance.AddSubmenu("Sequence Markers", &g_ApMarkers,
                          "Keyframe and camera-path appearance in Camera Sequence.");
  g_Appearance.AddSeparator("");
  g_Appearance.AddButton("Save Appearance to INI", [] {
    SaveSettings();
    SetStatusText("Appearance saved to SimpleCamera.ini");
  }, "Persist these appearance settings so they load next session.");

  // ---- Menu Layout ----
  g_ApLayout.AddFloat("Menu Position X", &g_MenuPosX, 0.0f, 0.9f, 0.005f, 3,
                      [](float) { g_Ctrl.SetPosition(g_MenuPosX, g_MenuPosY); },
                      "Horizontal position of the menu (fraction of screen width).");
  g_ApLayout.AddFloat("Menu Position Y", &g_MenuPosY, 0.0f, 0.85f, 0.005f, 3,
                      [](float) { g_Ctrl.SetPosition(g_MenuPosX, g_MenuPosY); },
                      "Vertical position of the menu (fraction of screen height).");
  g_ApLayout.AddFloat("Menu Scale", &g_MenuScale, 0.6f, 2.0f, 0.05f, 2,
                      [](float) { g_Ctrl.SetScale(g_MenuScale); },
                      "Overall menu size multiplier.");

  // ---- Menu Colours ---- (every slider recolours the menu live)
  auto recolor = [](int) { ApplyMenuTheme(); };
  g_ApColors.AddSeparator("Accent (bars, outline, scrollbar)");
  g_ApColors.AddInt("Accent Red", &g_MenuAccentR, 0, 255, 5, recolor, "Accent colour red channel.");
  g_ApColors.AddInt("Accent Green", &g_MenuAccentG, 0, 255, 5, recolor, "Accent colour green channel.");
  g_ApColors.AddInt("Accent Blue", &g_MenuAccentB, 0, 255, 5, recolor, "Accent colour blue channel.");
  g_ApColors.AddSeparator("Background (panel)");
  g_ApColors.AddInt("Bg Red", &g_MenuBgR, 0, 255, 5, recolor, "Panel background red channel.");
  g_ApColors.AddInt("Bg Green", &g_MenuBgG, 0, 255, 5, recolor, "Panel background green channel.");
  g_ApColors.AddInt("Bg Blue", &g_MenuBgB, 0, 255, 5, recolor, "Panel background blue channel.");
  g_ApColors.AddSeparator("Text");
  g_ApColors.AddInt("Text Red", &g_MenuTextR, 0, 255, 5, recolor, "Menu text red channel.");
  g_ApColors.AddInt("Text Green", &g_MenuTextG, 0, 255, 5, recolor, "Menu text green channel.");
  g_ApColors.AddInt("Text Blue", &g_MenuTextB, 0, 255, 5, recolor, "Menu text blue channel.");
  g_ApColors.AddSeparator("Selected Row");
  g_ApColors.AddInt("Selected Bg Red", &g_MenuSelR, 0, 255, 5, recolor, "Highlighted row fill red channel.");
  g_ApColors.AddInt("Selected Bg Green", &g_MenuSelG, 0, 255, 5, recolor, "Highlighted row fill green channel.");
  g_ApColors.AddInt("Selected Bg Blue", &g_MenuSelB, 0, 255, 5, recolor, "Highlighted row fill blue channel.");
  g_ApColors.AddSeparator("Selected Text");
  g_ApColors.AddInt("Selected Text Red", &g_MenuSelTextR, 0, 255, 5, recolor, "Highlighted row text red channel.");
  g_ApColors.AddInt("Selected Text Green", &g_MenuSelTextG, 0, 255, 5, recolor, "Highlighted row text green channel.");
  g_ApColors.AddInt("Selected Text Blue", &g_MenuSelTextB, 0, 255, 5, recolor, "Highlighted row text blue channel.");

  // ---- Sequence Markers ----
  g_ApMarkers.AddFloat("Keyframe Size", &g_SeqMarkerSize, 0.1f, 1.0f, 0.05f, 2, nullptr,
                       "Size of the in-world keyframe spheres in Camera Sequence.");
  g_ApMarkers.AddInt("Keyframe Red", &g_SeqMarkerR, 0, 255, 5, nullptr, "Normal keyframe marker red channel.");
  g_ApMarkers.AddInt("Keyframe Green", &g_SeqMarkerG, 0, 255, 5, nullptr, "Normal keyframe marker green channel.");
  g_ApMarkers.AddInt("Keyframe Blue", &g_SeqMarkerB, 0, 255, 5, nullptr, "Normal keyframe marker blue channel.");
  g_ApMarkers.AddInt("Path Red", &g_SeqPathR, 0, 255, 5, nullptr, "Camera path line red channel.");
  g_ApMarkers.AddInt("Path Green", &g_SeqPathG, 0, 255, 5, nullptr, "Camera path line green channel.");
  g_ApMarkers.AddInt("Path Blue", &g_SeqPathB, 0, 255, 5, nullptr, "Camera path line blue channel.");
}

// ============================================================
//  Camera Sequence — reorganized onto the new framework
// ============================================================
//
// The classic Sequence menu was one flat 18-row screen. Here it's regrouped:
// the few common actions (Capture / Play / Stop / Scrub) sit on the root, and
// everything else is a focused submenu. Variable-length lists (poses, events,
// sequences) are rebuilt each frame they're shown; per-item editors bind to
// stable mirror structs (s_pm / s_em) and push edits to the live element by
// index, so a vector reallocation (capture/add/sort) can never dangle a bound
// pointer.

static float SeqEventValueStep(int kindI) {
  switch (kindI) {
  case EFX_SHAKE_ENABLED:
  case EFX_SHAKE_PRESET:
  case EFX_SHAKE_STOP_STILL:
  case EFX_SHAKE_RANDOMIZE:
    return 1.0f; // discrete kinds step by whole units
  default:
    return 0.05f;
  }
}

// ---- Pose editor mirror <-> live keyframe ----
static void LoadPoseMirror() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editPose < 0 || s_editPose >= (int)s->poses.size()) return;
  const PoseKeyframe &p = s->poses[s_editPose];
  s_pm.t = p.t; s_pm.posX = p.posX; s_pm.posY = p.posY; s_pm.posZ = p.posZ;
  s_pm.pitch = p.pitch; s_pm.yaw = p.yaw; s_pm.roll = p.roll; s_pm.fov = p.fov;
  s_pm.easeI = (int)p.ease; s_pm.pathI = (int)p.path;
}
static void WritePoseMirror() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editPose < 0 || s_editPose >= (int)s->poses.size()) {
    s_seqPopRequested = true; // pose vanished under us — leave the editor
    return;
  }
  PoseKeyframe &p = s->poses[s_editPose];
  p.t = s_pm.t; p.posX = s_pm.posX; p.posY = s_pm.posY; p.posZ = s_pm.posZ;
  p.pitch = s_pm.pitch; p.yaw = s_pm.yaw; p.roll = s_pm.roll; p.fov = s_pm.fov;
  p.ease = (EaseType)s_pm.easeI; p.path = (PathType)s_pm.pathI;
}
static std::string PoseLockLabel() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editPose < 0 || s_editPose >= (int)s->poses.size()) return "None";
  const PoseKeyframe &p = s->poses[s_editPose];
  if (p.entityHandle != 0)
    return ENTITY::DOES_ENTITY_EXIST(p.entityHandle) ? "Locked" : "Locked (gone)";
  if (p.localOffsetX || p.localOffsetY || p.localOffsetZ || p.lockEntPitch ||
      p.lockEntYaw || p.lockEntRoll)
    return "Authored locked";
  return "None";
}
static void TogglePoseLock() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editPose < 0 || s_editPose >= (int)s->poses.size()) return;
  PoseKeyframe &p = s->poses[s_editPose];
  if (p.entityHandle != 0) {
    p.entityHandle = 0;
    p.localOffsetX = p.localOffsetY = p.localOffsetZ = 0.0f;
    p.lockEntPitch = p.lockEntYaw = p.lockEntRoll = 0.0f;
    SetStatusText("Entity lock cleared");
  } else if ((g_FollowMode == 1) ||
             (g_FollowMode == 2 && g_FollowTargetEntity != 0 &&
              ENTITY::DOES_ENTITY_EXIST(g_FollowTargetEntity))) {
    Sequence_CaptureLockForPose(p);
    SetStatusText(p.entityHandle != 0 ? "Locked to free-cam target" : "Lock failed");
  } else {
    p.localOffsetX = p.localOffsetY = p.localOffsetZ = 0.0f;
    p.lockEntPitch = p.lockEntYaw = p.lockEntRoll = 0.0f;
    SetStatusText("Aim & lock in Free Camera first");
  }
}
static void RecapturePose() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editPose < 0 || s_editPose >= (int)s->poses.size()) return;
  PoseKeyframe &p = s->poses[s_editPose];
  float px, py, pz, pi, ya, ro;
  GetCameraState(px, py, pz, pi, ya, ro);
  p.posX = px; p.posY = py; p.posZ = pz;
  p.pitch = pi; p.yaw = ya; p.roll = ro; p.fov = g_CamFOV;
  Sequence_CaptureLockForPose(p);
  LoadPoseMirror();
  SetStatusText("Pose recaptured from live camera");
}
static void DeleteEditPose() {
  if (s_editPose >= 0) {
    Sequence_DeletePose(s_editPose);
    SetStatusText("Keyframe deleted");
  }
  s_editPose = -1;
  s_seqPopRequested = true;
}

// ---- Event editor mirror <-> live event ----
static void LoadEventMirror() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editEvent < 0 || s_editEvent >= (int)s->events.size()) return;
  const EffectEvent &e = s->events[s_editEvent];
  s_em.t = e.t; s_em.value = e.value; s_em.kindI = (int)e.kind; s_em.ramp = e.ramp;
}
static void WriteEventMirror() {
  CameraSequence *s = Sequence_Active();
  if (!s || s_editEvent < 0 || s_editEvent >= (int)s->events.size()) {
    s_seqPopRequested = true;
    return;
  }
  EffectEvent &e = s->events[s_editEvent];
  e.t = s_em.t; e.value = s_em.value; e.kind = (EffectKind)s_em.kindI; e.ramp = s_em.ramp;
}
static void DeleteEditEvent() {
  if (s_editEvent >= 0) {
    Sequence_DeleteEvent(s_editEvent);
    SetStatusText("Event deleted");
  }
  s_editEvent = -1;
  s_seqPopRequested = true;
}
static void AddEventAndEdit() {
  float t = Sequence_CurrentTime();
  Sequence_AddEvent(EFX_SHAKE_ENABLED, t, 1.0f, false);
  CameraSequence *s = Sequence_Active();
  int idx = -1;
  if (s)
    for (int i = 0; i < (int)s->events.size(); ++i) {
      const EffectEvent &e = s->events[i];
      if (e.kind == EFX_SHAKE_ENABLED && fabsf(e.t - t) < 0.001f &&
          e.value == 1.0f && !e.ramp) { idx = i; break; }
    }
  if (idx >= 0) { s_editEvent = idx; g_Ctrl.Push(&g_SeqEventEdit); }
  else SetStatusText("Event added");
}

static void BakeLockedPoses() {
  CameraSequence *s = Sequence_Active();
  int n = 0;
  if (s)
    for (PoseKeyframe &p : s->poses)
      if (p.entityHandle != 0 && ENTITY::DOES_ENTITY_EXIST(p.entityHandle)) {
        Vector3 w = invoke<Vector3>(0x1899F328B0E12848, p.entityHandle,
                                    p.localOffsetX, p.localOffsetY, p.localOffsetZ);
        p.posX = w.x; p.posY = w.y; p.posZ = w.z; ++n;
      }
  char b[64];
  sprintf_s(b, "Baked %d locked poses to world coords", n);
  SetStatusText(b);
}

// Hover marker on the entity under the crosshair while in Aimed mode with no
// lock yet — so the user sees what "Lock Aimed Entity" will grab.
static void DrawSeqHoverMarker() {
  if (g_FollowMode != 2 || g_FollowTargetEntity != 0) return;
  int e = RaycastEntityFromCamera();
  if (e == 0 || !ENTITY::DOES_ENTITY_EXIST(e)) return;
  Vector3 p = ENTITY::GET_ENTITY_COORDS(e, TRUE);
  GRAPHICS::DRAW_MARKER(0, p.x, p.y, p.z + 1.25f, 0, 0, 0, 0, 0, 0, 0.4f, 0.4f,
                        0.4f, 255, 255, 255, 200, TRUE, TRUE, 2, FALSE, NULL,
                        NULL, FALSE);
}

// ---- Dynamic list (re)builders ----
static void RebuildPoseList() {
  int sel = g_SeqPoses.selected, scroll = g_SeqPoses.scrollOffset;
  g_SeqPoses.items.clear();
  g_SeqPoses.AddButton("+ Capture Pose at camera", [] {
    int i = Sequence_CapturePoseAtCurrentTime();
    if (i >= 0) SetStatusText("Pose captured");
  }, "Add a keyframe at the camera's current position and angle.").rightLabel = "F6";
  CameraSequence *s = Sequence_Active();
  int n = s ? (int)s->poses.size() : 0;
  if (n >= 1)
    g_SeqPoses.AddFloat("Total Duration (s)", &s_totalDur, 0.1f, 100000.0f, 0.1f,
                        2, [](float v) {
                          float cur = Sequence_TotalDuration();
                          if (cur > 0.01f && v > 0.01f) Sequence_ScaleTimes(v / cur);
                        }, "Stretch or compress every keyframe + event time at once.");
  for (int i = 0; i < n; ++i) {
    char label[24]; sprintf_s(label, "Keyframe %d", i);
    g_SeqPoses.AddButton(label, [i] { s_editPose = i; g_Ctrl.Push(&g_SeqPoseEdit); },
                         "Open this keyframe to edit its pose, timing, easing and lock.")
        .valueGetter = [i] {
          CameraSequence *s2 = Sequence_Active();
          if (!s2 || i >= (int)s2->poses.size()) return std::string();
          const PoseKeyframe &p = s2->poses[i];
          char b[96];
          sprintf_s(b, "t=%.2f fov=%.0f %s/%s", p.t, p.fov, EaseName(p.ease),
                    PathName(p.path));
          return std::string(b);
        };
  }
  int cnt = (int)g_SeqPoses.items.size();
  if (sel >= cnt) sel = cnt - 1; if (sel < 0) sel = 0;
  g_SeqPoses.selected = sel; g_SeqPoses.scrollOffset = scroll;
}
static void RebuildEventList() {
  int sel = g_SeqEvents.selected, scroll = g_SeqEvents.scrollOffset;
  g_SeqEvents.items.clear();
  g_SeqEvents.AddButton("+ Add event at current time", [] { AddEventAndEdit(); },
                        "Create a new timed effect change at the playhead.");
  CameraSequence *s = Sequence_Active();
  int n = s ? (int)s->events.size() : 0;
  for (int i = 0; i < n; ++i) {
    char label[24]; sprintf_s(label, "Event %d", i);
    g_SeqEvents.AddButton(label, [i] { s_editEvent = i; g_Ctrl.Push(&g_SeqEventEdit); },
                          "Open this event to change its effect, value and timing.")
        .valueGetter = [i] {
          CameraSequence *s2 = Sequence_Active();
          if (!s2 || i >= (int)s2->events.size()) return std::string();
          const EffectEvent &e = s2->events[i];
          char b[96];
          sprintf_s(b, "t=%.2f %s=%.2f %s", e.t, EffectName(e.kind), e.value,
                    e.ramp ? "ramp" : "snap");
          return std::string(b);
        };
  }
  int cnt = (int)g_SeqEvents.items.size();
  if (sel >= cnt) sel = cnt - 1; if (sel < 0) sel = 0;
  g_SeqEvents.selected = sel; g_SeqEvents.scrollOffset = scroll;
}
static void RebuildSeqList() {
  int sel = g_SeqList.selected, scroll = g_SeqList.scrollOffset;
  g_SeqList.items.clear();
  g_SeqList.AddButton("New Sequence", [] {
    Sequence_New("Untitled");
    SetStatusText("New sequence created");
  }, "Create a new, empty sequence and make it active.");
  g_SeqList.AddButton("Delete Active", [] {
    DWORD now = GetTickCount();
    if (now < s_seqDelArmed) {
      Sequence_DeleteActive(); s_seqDelArmed = 0; SetStatusText("Sequence deleted");
    } else {
      s_seqDelArmed = now + 3000; SetStatusText("Press again to confirm delete");
    }
  }, "Delete the current sequence. Press twice within 3s to confirm.")
      .valueGetter = [] { return std::string(GetTickCount() < s_seqDelArmed ? "Confirm?" : ""); };
  g_SeqList.AddButton("Save All to INI", [] {
    Sequence_SaveAll(); SetStatusText("Saved to INI");
  }, "Save every sequence to disk (SimpleCamera_Sequences.ini).");
  g_SeqList.AddSeparator("Select Active");
  int n = Sequence_Count();
  for (int i = 0; i < n; ++i) {
    CameraSequence *cs = Sequence_At(i);
    std::string nm = cs ? cs->name : std::string("?");
    g_SeqList.AddButton(nm, [i] { Sequence_SetActive(i); SetStatusText("Active sequence set"); },
                        "Make this the active sequence to edit and play.")
        .valueGetter = [i] { return std::string(i == Sequence_ActiveIndex() ? "Active" : ""); };
  }
  int cnt = (int)g_SeqList.items.size();
  if (sel >= cnt) sel = cnt - 1; if (sel < 0) sel = 0;
  g_SeqList.selected = sel; g_SeqList.scrollOffset = scroll;
}
static void BuildSeqPlayback() {
  g_SeqPlayback.items.clear();
  CameraSequence *s = Sequence_Active();
  if (!s) { g_SeqPlayback.AddLabel("No active sequence"); g_SeqPlayback.selected = 0; return; }
  g_SeqPlayback.AddToggle("Loop", &s->loop, nullptr,
                          "Repeat the sequence from the start when it reaches the end.");
  g_SeqPlayback.AddFloat("Playback Speed", &s->playbackSpeed, 0.05f, 8.0f, 0.05f, 2, nullptr,
                         "Playback speed multiplier. 2.0 plays twice as fast, 0.5 half.");
  g_SeqPlayback.AddButton("Close Loop", [] {
    int i = Sequence_CloseLoop();
    if (i >= 0) SetStatusText(Sequence_IsLoopClosed() ? "Loop closed" : "Closing keyframe added");
  }, "Add/snap a closing keyframe so a looped shot wraps seamlessly.")
      .valueGetter = [] {
    LoopGap g;
    if (Sequence_GetLoopGap(&g)) {
      char b[48];
      sprintf_s(b, "d=%.2fm/%.0fdeg", g.posDist, g.pitchDelta + g.yawDelta + g.rollDelta);
      return std::string(b);
    }
    return std::string("Press");
  };
  g_SeqPlayback.AddToggle("Show Markers", &g_SequenceShowMarkers, nullptr,
                          "Show the keyframe spheres and path preview in the world "
                          "(hide them before recording).");
  g_SeqPlayback.selected = 0;
}

// Snap the render preset mirrors to the live globals (called on entry).
static void SyncRenderMirrors() {
  int bi = 0; float bd = 1e9f;
  for (int i = 0; i < kRenderFpsCount; ++i) {
    float d = fabsf(kRenderFps[i] - g_RenderFps);
    if (d < bd) { bd = d; bi = i; }
  }
  s_fpsIdx = bi;
  int bj = 0, bb = 1 << 30;
  for (int i = 0; i < kRenderBlurCount; ++i) {
    int d = kRenderBlur[i] - g_RenderBlurSamples; if (d < 0) d = -d;
    if (d < bb) { bb = d; bj = i; }
  }
  s_blurIdx = bj;
}

// Rebuilt each frame it's shown so options that don't apply to the current mode
// (Synced vs Camera-led, blur off, PNG) gray out + read "n/a" instead of letting
// you tweak settings that have no effect.
static void RebuildSeqRender() {
  int sel = g_SeqRender.selected, scroll = g_SeqRender.scrollOffset;
  g_SeqRender.items.clear();

  const bool synced = g_RenderSyncWorld != 0;
  const bool blurOn = g_RenderBlurSamples > 1;
  const bool jpeg = g_RenderFormat == 1;

  g_SeqRender.AddList("Render Mode", &g_RenderSyncWorld,
                      {"Camera-led (static)", "Synced (live world)"}, nullptr,
                      "Camera-led scrubs a static world; Synced plays the world "
                      "+ camera on one clock.");
  g_SeqRender.AddList("Output FPS", &s_fpsIdx,
                      {"24", "25", "30", "48", "50", "60", "120", "240"},
                      [](int i) { g_RenderFps = kRenderFps[i]; },
                      "Frame rate of the exported sequence (sets the total frame count).")
      .valueGetter = [] {
        char b[48];
        int frames = (int)(Sequence_TotalDuration() * g_RenderFps + 0.5f);
        sprintf_s(b, "%d fps - %d fr", (int)g_RenderFps, frames);
        return std::string(b);
      };
  {
    auto &it = g_SeqRender.AddInt("Settle Frames", &g_RenderSettleFrames, 0, 60, 1, nullptr,
        "Frames to let the scene settle (TAA/streaming) before grabbing. "
        "Camera-led only - not used in Synced mode.");
    it.enabled = !synced;
    if (synced) it.valueGetter = [] { return std::string("n/a"); };
  }
  g_SeqRender.AddInt("Flush Frames", &g_RenderFlushFrames, 0, 20, 1, nullptr,
                     "Extra clean frames after the progress banner clears, before each grab.");
  g_SeqRender.AddList("Motion Blur", &s_blurIdx,
                      {"Off", "2", "4", "8", "16", "32", "64"},
                      [](int i) { g_RenderBlurSamples = kRenderBlur[i]; },
                      "Sub-samples accumulated per output frame (Off = sharp). Requires "
                      "the IgcsDof.fx shader to be enabled in the ReShade effects menu.");
  {
    auto &it = g_SeqRender.AddFloat("Shutter", &g_RenderShutter, 0.05f, 1.0f, 0.05f, 2, nullptr,
        "Shutter angle for motion blur. Used by Camera-led accumulation only, "
        "and only when Motion Blur is on.");
    it.enabled = blurOn && !synced;
    it.valueGetter = [] {
      if (g_RenderBlurSamples <= 1 || g_RenderSyncWorld) return std::string("n/a");
      char b[16]; sprintf_s(b, "%d deg", (int)(g_RenderShutter * 360.0f + 0.5f));
      return std::string(b);
    };
  }
  {
    auto &it = g_SeqRender.AddFloat("Highlight Boost", &g_RenderHighlightBoost, 0.0f, 0.95f, 0.05f, 2, nullptr,
        "Extra brightness lift on blur streaks. Only used when Motion Blur is on.");
    it.enabled = blurOn;
    it.valueGetter = [] {
      if (g_RenderBlurSamples <= 1) return std::string("n/a");
      char b[8]; sprintf_s(b, "%d%%", (int)(g_RenderHighlightBoost * 100.0f + 0.5f));
      return std::string(b);
    };
  }
  g_SeqRender.AddList("Format", &g_RenderFormat, {"PNG (lossless)", "JPEG"}, nullptr,
                      "Image format: PNG is lossless/large, JPEG is smaller/lossy.");
  {
    auto &it = g_SeqRender.AddInt("JPEG Quality", &g_RenderJpegQuality, 10, 100, 5, nullptr,
        "JPEG quality 10-100. Only used when Format is JPEG.");
    it.enabled = jpeg;
    it.valueGetter = [] {
      if (g_RenderFormat != 1) return std::string("n/a");
      char b[8]; sprintf_s(b, "%d", g_RenderJpegQuality); return std::string(b);
    };
  }
  g_SeqRender.AddFloat("World Slow-mo", &g_RenderSlowmo, 0.0f, 1.0f, 0.01f, 2, nullptr,
                       "Time scale during capture. Auto (0) picks a safe value per frame.")
      .valueGetter = [] {
        if (g_RenderSlowmo <= 0.0001f) return std::string("Auto");
        char b[8]; sprintf_s(b, "%d%%", (int)(g_RenderSlowmo * 100.0f + 0.5f));
        return std::string(b);
      };
  g_SeqRender.AddButton("Start Render", [] {
    if (FxCapture_AddonPresent()) ProcessRenderToImages();
    else SetStatusText("Rendering needs ReShade + IgcsConnector addon");
  }, "Render the sequence to a numbered image folder. Needs the ReShade + "
     "IgcsConnector capture addon.")
      .valueGetter = [] {
        return std::string(FxCapture_AddonPresent() ? "Press Enter" : "Addon missing");
      };

  int cnt = (int)g_SeqRender.items.size();
  if (sel >= cnt) sel = cnt - 1;
  if (sel < 0) sel = 0;
  g_SeqRender.selected = sel;
  g_SeqRender.scrollOffset = scroll;
}

static void BuildSeqTree() {
  g_EffectOpts.clear();
  for (int k = 0; k < EFX_COUNT; ++k) g_EffectOpts.push_back(EffectName((EffectKind)k));

  // The sequence root rows now live in the unified root (RebuildRoot); only the
  // editors / lists / follow / render sub-pages are built here.

  // ---- Pose editor (mirror-bound) ----
  g_SeqPoseEdit.onPush = [] { LoadPoseMirror(); };
  g_SeqPoseEdit.onPop = [] { Sequence_SortByTime(); };
  g_SeqPoseEdit.AddFloat("Time (s)", &s_pm.t, 0.0f, 100000.0f, 0.1f, 2, nullptr,
                         "When this keyframe happens on the timeline, in seconds.");
  g_SeqPoseEdit.AddFloat("Pos X", &s_pm.posX, -1000000.0f, 1000000.0f, 0.5f, 2, nullptr,
                         "World X position of the camera at this keyframe.");
  g_SeqPoseEdit.AddFloat("Pos Y", &s_pm.posY, -1000000.0f, 1000000.0f, 0.5f, 2, nullptr,
                         "World Y position of the camera at this keyframe.");
  g_SeqPoseEdit.AddFloat("Pos Z", &s_pm.posZ, -1000000.0f, 1000000.0f, 0.5f, 2, nullptr,
                         "World Z (height) of the camera at this keyframe.");
  g_SeqPoseEdit.AddFloat("Pitch", &s_pm.pitch, -360.0f, 360.0f, 1.0f, 2, nullptr,
                         "Up/down look angle at this keyframe (degrees).");
  g_SeqPoseEdit.AddFloat("Yaw", &s_pm.yaw, -360.0f, 360.0f, 1.0f, 2, nullptr,
                         "Left/right look angle at this keyframe (degrees).");
  g_SeqPoseEdit.AddFloat("Roll", &s_pm.roll, -360.0f, 360.0f, 1.0f, 2, nullptr,
                         "Side tilt (Dutch angle) at this keyframe (degrees).");
  g_SeqPoseEdit.AddFloat("FOV", &s_pm.fov, 5.0f, 130.0f, 1.0f, 1, nullptr,
                         "Lens zoom at this keyframe. Lower = more zoom.");
  g_SeqPoseEdit.AddList("Ease", &s_pm.easeI, {"Linear", "In-Out", "In", "Out", "Hold"},
                        nullptr,
                        "How motion eases around this keyframe. In-Out = smooth start & stop.");
  g_SeqPoseEdit.AddList("Path", &s_pm.pathI, {"Linear", "Spline"}, nullptr,
                        "Straight line to the next keyframe, or a smooth Spline curve.");
  g_SeqPoseEdit.AddButton("Entity Lock", [] { TogglePoseLock(); },
                          "Lock this keyframe to ride along with the free-cam's target entity.")
      .valueGetter = [] { return PoseLockLabel(); };
  g_SeqPoseEdit.AddButton("Recapture from live", [] { RecapturePose(); },
                          "Overwrite this keyframe with the camera's current pose.");
  g_SeqPoseEdit.AddButton("Delete Keyframe", [] { DeleteEditPose(); },
                          "Remove this keyframe from the sequence.");

  // ---- Event editor (mirror-bound) ----
  g_SeqEventEdit.onPush = [] { LoadEventMirror(); };
  g_SeqEventEdit.onPop = [] { Sequence_SortByTime(); };
  g_SeqEventEdit.AddFloat("Time (s)", &s_em.t, 0.0f, 100000.0f, 0.1f, 2, nullptr,
                          "When this effect change fires on the timeline.");
  g_SeqEventEdit.AddList("Effect", &s_em.kindI, g_EffectOpts, nullptr,
                         "Which property this event changes (shake, world speed, etc.).");
  s_pEventValue = &g_SeqEventEdit.AddFloat("Value", &s_em.value, -100000.0f,
                                           100000.0f, 0.05f, 3, nullptr,
                                           "The value applied for the chosen effect.");
  g_SeqEventEdit.AddToggle("Ramp (lerp from prev)", &s_em.ramp, nullptr,
                           "Snap to the value instantly, or smoothly ramp from the previous event.");
  g_SeqEventEdit.AddButton("Delete Event", [] { DeleteEditEvent(); },
                           "Remove this effect event.");

  // ---- Follow & Entity Lock ----
  g_SeqFollow.AddList("Follow Mode", &g_FollowMode,
                      {"None", "Player", "Aimed Entity"},
                      [](int m) { if (m != 2) g_FollowTargetEntity = 0; },
                      "What the camera tracks: nothing, the player, or a locked entity.");
  g_SeqFollow.AddToggle("Rigid Mode", &g_FollowRigidMode, nullptr,
                        "Inherit the target's rotation, not just its position.");
  g_SeqFollow.AddToggle("Show Marker", &g_ShowLockedEntityMarker, nullptr,
                        "Draw a marker on the locked entity.");
  g_SeqFollow.AddButton("Lock Aimed Entity (raycast)", [] {
    int e = RaycastEntityFromCamera();
    if (e) { g_FollowTargetEntity = e; g_FollowMode = 2; SetStatusText("Locked onto entity"); }
    else SetStatusText("No entity found. Aim closer.");
  }, "Point the camera at a ped/vehicle/object and lock onto it.");
  g_SeqFollow.AddButton("Lock to Nearest Vehicle", [] {
    float x, y, z, pi, ya, ro;
    GetCameraState(x, y, z, pi, ya, ro);
    int v = VEHICLE::GET_CLOSEST_VEHICLE(x, y, z, 30.0f, 0, 70);
    if (v && ENTITY::DOES_ENTITY_EXIST(v)) { g_FollowTargetEntity = v; g_FollowMode = 2; SetStatusText("Locked nearest vehicle"); }
    else SetStatusText("No vehicle within 30m");
  }, "Lock the closest vehicle - works at close range where the raycast can fail.");
  g_SeqFollow.AddButton("Lock to Player's Vehicle", [] {
    Ped p = PLAYER::PLAYER_PED_ID();
    int v = PED::IS_PED_IN_ANY_VEHICLE(p, FALSE) ? PED::GET_VEHICLE_PED_IS_IN(p, FALSE) : 0;
    if (v && ENTITY::DOES_ENTITY_EXIST(v)) { g_FollowTargetEntity = v; g_FollowMode = 2; SetStatusText("Locked player vehicle"); }
    else SetStatusText("Player isn't in a vehicle");
  }, "Lock onto the car you're currently driving.");
  g_SeqFollow.AddButton("Unlock Entity", [] { g_FollowTargetEntity = 0; SetStatusText("Entity unlocked"); },
                        "Release the locked entity.");
  g_SeqFollow.AddSeparator("Keyframe Locks");
  g_SeqFollow.AddButton("Apply Lock to All Keyframes", [] {
    int t = 0;
    if (g_FollowMode == 1) t = PLAYER::PLAYER_PED_ID();
    else if (g_FollowMode == 2) t = g_FollowTargetEntity;
    if (!t || !ENTITY::DOES_ENTITY_EXIST(t)) { SetStatusText("Lock free-cam to a target first"); return; }
    int n = Sequence_ApplyLockToAll(t);
    char b[64]; sprintf_s(b, "Locked %d keyframes to target", n); SetStatusText(b);
  }, "Re-anchor every keyframe to the currently locked target entity.");
  g_SeqFollow.AddButton("Clear All Keyframe Locks", [] {
    int n = Sequence_ClearAllLocks();
    char b[64]; sprintf_s(b, "Cleared lock from %d keyframes", n); SetStatusText(b);
  }, "Remove entity locks from every keyframe (world positions are kept).");
  g_SeqFollow.AddButton("Bake Locked Poses to World", [] { BakeLockedPoses(); },
                        "Freeze locked keyframes at the entity's current spot (world coords).");

  // ---- Sequences + Playback + Render are (re)built on entry ----
  g_SeqList.onPush = [] { RebuildSeqList(); };
  g_SeqPlayback.onPush = [] { BuildSeqPlayback(); };
  g_SeqRender.onPush = [] { SyncRenderMirrors(); RebuildSeqRender(); };
}

// ============================================================
//  Unified root — Camera Mode switcher + per-mode content
// ============================================================

// The live mode, derived from the camera state (not a stored flag).
static int CurrentMode() {
  if (Sequence_IsInMode()) return 2;
  if (g_FreeCamActive) return 1;
  return 0;
}

static const char *ModeName(int m) {
  return m == 2 ? "Camera Sequence" : m == 1 ? "Free Camera" : "Off";
}

// Switch to mode `m` (0 Off, 1 Free Camera, 2 Sequence), tearing down whatever's
// active first. This is the ONE place mode changes — no scattered exit buttons.
static void ApplyMode(int m) {
  int cur = CurrentMode();
  if (m == cur) return;
  // Leave the current mode.
  if (cur == 2) { Sequence_ExitMode(); g_CameraMode = -1; }
  else if (cur == 1) { DestroyFreeCamera(); }
  // Enter the new one.
  if (m == 1) {
    g_CameraMode = 0;
    InitFreeCamera();
    SetStatusText("Free Camera on");
  } else if (m == 2) {
    g_CameraMode = 1;
    Sequence_EnterMode();
    SetStatusText("Camera Sequence on");
  } else {
    g_CameraMode = -1;
    SetStatusText("Camera off");
  }
}

// Rebuild the root's rows to match the active mode (called each frame it's
// shown). Row 0 is always the Camera Mode switcher.
static void RebuildRoot() {
  int sel = g_Root.selected, scroll = g_Root.scrollOffset;
  g_Root.items.clear();

  int mode = CurrentMode();
  g_Root.subtitle = ModeName(mode);

  g_Root.AddList("Camera Mode", &s_mode, {"Off", "Free Camera", "Camera Sequence"},
                 [](int m) { ApplyMode(m); },
                 "Switch the active camera mode.");

  if (mode == 1) {
    // ---- Free Camera ----
    g_Root.AddSeparator("");
    g_Root.AddSubmenu("Movement", &g_Movement,
                      "Speed, sensitivity, collision, drone, follow.");
    g_Root.AddSubmenu("Lens", &g_Lens, "Field of view and roll.");
    g_Root.AddSubmenu("Depth of Field", &g_DoF, "Focus and bokeh range.");
    g_Root.AddSubmenu("Camera Effects", &g_Effects, "Procedural handheld shake.");
    g_Root.AddSubmenu("Misc", &g_Misc, "Player linkage and quick actions.");
    g_Root.AddSubmenu("World & Scene", &g_World,
                      "Time, weather, auto-drive, HUD, player.");
    g_Root.AddSubmenu("Appearance", &g_Appearance,
                      "Menu position, scale, accent colour, and sequence marker/path look.");
    g_Root.AddSeparator("");
    g_Root.AddButton("Save Settings", [] {
      SaveSettings();
      SetStatusText("Settings saved to SimpleCamera.ini");
    }, "Write the current setup to SimpleCamera.ini.");
  } else if (mode == 2) {
    // ---- Camera Sequence ----
    g_Root.AddSeparator("");
    g_Root.AddButton("Capture Pose", [] {
      int i = Sequence_CapturePoseAtCurrentTime();
      if (i >= 0) SetStatusText("Pose captured");
    }, "Add a keyframe at the camera's current pose (hotkey F6).").valueGetter = [] {
      CameraSequence *s = Sequence_Active();
      char b[24]; sprintf_s(b, "F6 - %d kf", s ? (int)s->poses.size() : 0);
      return std::string(b);
    };
    g_Root.AddButton("Play / Pause", [] { Sequence_TogglePlay(); },
                     "Start or pause playback of the sequence (hotkey F7).")
        .valueGetter = [] { return std::string(Sequence_IsPlaying() ? "Playing" : "Paused"); };
    g_Root.AddButton("Stop", [] { Sequence_Stop(); SetStatusText("Stopped"); },
                     "Stop playback and rewind to the start (hotkey F8).");
    float dur = Sequence_TotalDuration();
    g_Root.AddFloat("Scrub Time (s)", &s_scrub, 0.0f, dur > 0.1f ? dur : 0.1f, 0.1f, 2,
                    [](float v) { Sequence_SetCurrentTime(v); },
                    "Move the playhead through the sequence by hand to preview a moment.");
    g_Root.AddSeparator("");
    g_Root.AddSubmenu("Pose Keyframes", &g_SeqPoses,
                      "The camera poses that make up the shot. Add, edit and delete keyframes.")
        .valueGetter = [] {
          CameraSequence *s = Sequence_Active();
          char b[16]; sprintf_s(b, "%d", s ? (int)s->poses.size() : 0);
          return std::string(b);
        };
    g_Root.AddSubmenu("Effect Events", &g_SeqEvents,
                      "Timed effect changes along the timeline (shake, world speed, etc.).")
        .valueGetter = [] {
          CameraSequence *s = Sequence_Active();
          char b[16]; sprintf_s(b, "%d", s ? (int)s->events.size() : 0);
          return std::string(b);
        };
    g_Root.AddSubmenu("Playback Settings", &g_SeqPlayback,
                      "Loop, playback speed, loop-closing and in-world markers.");
    g_Root.AddSubmenu("Follow & Entity Lock", &g_SeqFollow,
                      "Lock the camera or keyframes to a moving ped/vehicle.");
    g_Root.AddSubmenu("Sequences", &g_SeqList,
                      "Create, pick, delete and save your sequences.")
        .valueGetter = [] {
          CameraSequence *s = Sequence_Active();
          char b[48];
          sprintf_s(b, "%s [%d/%d]", s ? s->name.c_str() : "(none)",
                    Sequence_ActiveIndex() + 1, Sequence_Count());
          return std::string(b);
        };
    g_Root.AddSubmenu("Render to Images", &g_SeqRender,
                      "Export the sequence as a numbered image sequence (needs ReShade addon).");
    g_Root.AddSubmenu("World & Scene", &g_World,
                      "Time, weather, HUD and player visibility.");
    g_Root.AddSubmenu("Appearance", &g_Appearance,
                      "Menu position, scale, accent colour, and sequence marker/path look.");
  } else {
    // ---- Off ---- (no camera engaged, but world/menu config is still useful)
    g_Root.AddSeparator("");
    g_Root.AddSubmenu("World & Scene", &g_World,
                      "Time, weather, HUD and player visibility.");
    g_Root.AddSubmenu("Appearance", &g_Appearance,
                      "Menu position, scale, accent colour, and sequence marker/path look.");
  }

  int cnt = (int)g_Root.items.size();
  if (sel >= cnt) sel = cnt - 1;
  if (sel < 0) sel = 0;
  g_Root.selected = sel;
  g_Root.scrollOffset = scroll;
}

// Per-frame sequence upkeep: keep live lists/mirrors in step with the data.
// Called from SCMenu_Update BEFORE the controller's Update so input acts on the
// freshly-rebuilt items.
static void SeqMenu_FrameSync() {
  gtam::Menu *cur = g_Ctrl.Current();

  // Mode mirror + always-visible mode indicator in the footer.
  s_mode = CurrentMode();
  g_Ctrl.SetFooterText(std::string("Mode: ") + ModeName(s_mode));

  // Root scrub mirrors the live playhead.
  s_scrub = Sequence_CurrentTime();

  if (cur == &g_Root) { RebuildRoot(); }
  else if (cur == &g_SeqPoses) { s_totalDur = Sequence_TotalDuration(); RebuildPoseList(); }
  else if (cur == &g_SeqEvents) { RebuildEventList(); }
  else if (cur == &g_SeqList) { RebuildSeqList(); }
  else if (cur == &g_SeqRender) { RebuildSeqRender(); }
  else if (cur == &g_SeqPoseEdit) { WritePoseMirror(); }
  else if (cur == &g_SeqEventEdit) {
    WriteEventMirror();
    if (s_pEventValue) s_pEventValue->fStep = SeqEventValueStep(s_em.kindI);
  } else if (cur == &g_SeqFollow) {
    DrawSeqHoverMarker();
  }
}

// ============================================================
//  Public API
// ============================================================

void SCMenu_Init() {
  if (g_Built) return;
  BuildTree();
  BuildSeqTree();
  g_Built = true;
}

void SCMenu_Toggle() {
  if (!g_Built) SCMenu_Init();
  g_Ctrl.Toggle(&g_Root); // single unified menu (mode switcher at the top)
}

bool SCMenu_IsOpen() { return g_Ctrl.IsOpen(); }

void SCMenu_Update() {
  if (!g_Built) return;

  // When the Time & Weather page first comes into view, sync the clock fields
  // to the live game time (matches the classic menu's open-time refresh).
  static gtam::Menu *s_prev = nullptr;
  gtam::Menu *cur = g_Ctrl.Current();
  if (cur == &g_Time && s_prev != &g_Time && !g_TimePaused) {
    g_TimeHour = TIME::GET_CLOCK_HOURS();
    g_TimeMinute = TIME::GET_CLOCK_MINUTES();
  }
  s_prev = cur;

  // Keep sequence lists / editors in step with the live data before input.
  SeqMenu_FrameSync();

  bool visible = g_Ctrl.Update();

  // Deferred Back() requested from an editor (e.g. Delete) — done after Update
  // so we don't mutate the stack mid-frame.
  if (s_seqPopRequested) {
    s_seqPopRequested = false;
    g_Ctrl.Back();
  }

  // Tell the free camera a menu is up so it suppresses input it shares with
  // menu navigation (same contract the classic menu uses).
  g_MenuOpen = visible;
}
