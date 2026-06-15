/*
        GTA V Free Camera / Photo Mode Plugin
        Main Script Loop

        Entry point for the ScriptHookV runtime. Runs the main loop
        that checks for menu activation and updates the free camera each frame.
*/

#include "script.h"
#include "camera.h"
#include "fx_capture.h"
#include "igcs_bridge.h"
#include "keyboard.h"
#include "menu.h"
#include "scmenu.h"
#include "sequence.h"
#include "vehicleclip.h"

#include "external\scripthook_sdk\inc\natives.h"


#pragma warning(disable : 4244 4305)

// ============================================================
//  Main Loop
// ============================================================

void main() {
  // Detect if running in FiveM
  DetectFiveM();

  // Load INI settings — populates camera/shake/DoF/misc tunables from
  // SimpleCamera.ini, falling back to the global initializers in camera.cpp
  // when a key is absent. Must run before the main loop so a saved setup is
  // live from the first frame.
  LoadSettings();
  // Load any persisted camera sequences
  Sequence_LoadAll();

  // Map the shared capture channel so the ReShade addon can write frames.
  FxCapture_Init();

  // Build the new (gtam framework) menu tree. During migration it lives on F11
  // alongside the classic F5 menu so both can be compared in-game.
  SCMenu_Init();

  // Never start with the free camera already engaged, regardless of what was
  // persisted — the user always toggles it on explicitly (or via the picker).
  g_FreeCamActive = false;

  while (true) {
    // Sample the vehicle path while a recording is active (no-op otherwise).
    VehicleClip_RecordTick();

    if (VehicleClip_IsRecording()) {
      // Manual-drive recording session: the free camera is suspended and the
      // player has the wheel. The menu and camera dispatch are paused; only the
      // Menu key is live, and it STOPS the take (restoring the free camera).
      VehicleClip_DrawRecordingBanner();
      if (IsMenuTogglePressed() || IsControllerMenuCombo()) {
        MenuBeep();
        VehicleClip_StopRecording();
        ConsumeMenuToggle(); // same release must not re-open the menu next frame
      }
    } else {
      // Check for menu toggle — F5 on keyboard, or LB+RB on a controller (a pad
      // has no menu key otherwise). Frame-driven: toggle on press, drive every
      // frame below.
      if (IsMenuTogglePressed() || IsControllerMenuCombo()) {
        MenuBeep();
        SCMenu_Toggle();
      }
      SCMenu_Update();

      // Controller LB+B exits Free Camera straight back to the picker, so pad
      // users can bail without opening the menu.
      if (g_CameraMode == 0 && g_FreeCamActive && IsControllerExitCombo()) {
        MenuBeep();
        DestroyFreeCamera();
        g_CameraMode = -1;
      }

      // F10 — Phase 1 capture test: ask the ReShade addon to write the current
      // frame to a PNG under SimpleCamera_Captures. Press while flying (menu
      // closed) for a clean frame.
      if (IsKeyJustUp(VK_F10)) {
        char savedPath[MAX_PATH];
        if (FxCapture_CaptureTest(savedPath, sizeof(savedPath))) {
          SetStatusText("Frame capture requested");
        } else {
          SetStatusText("Capture channel unavailable (addon not loaded?)");
        }
      }

      // Mode dispatch — Free Camera and Camera Sequence are mutually exclusive
      if (g_CameraMode == 1 && Sequence_IsInMode()) {
        Sequence_FrameTick();
      } else if (g_FreeCamActive) {
        UpdateFreeCamera();
      }
    }

    // Drive the player's vehicle via AI when Auto Drive is engaged (no-op
    // otherwise). Runs alongside the free camera so you can film a self-driving
    // car hands-free.
    UpdateAutoDrive();

    // Update time/weather overrides
    UpdateTimeWeather();

    // Update on-screen status text
    UpdateStatusText();

    // Global effects (freeze world, info overlay)
    UpdateGlobalEffects();

    // IGCS belongs to Free Camera only — its ReShade screenshot workflow
    // makes no sense when sequence playback owns the camera. In Sequence
    // mode, advertise cameraEnabled=false so any active IGCS session on
    // the ReShade side releases its lock cleanly.
    if (g_CameraMode != 1) {
      IGCS_TryConnect();
      if (IGCS_IsConnected()) {
        float px, py, pz, pitch, yaw, roll;
        GetCameraState(px, py, pz, pitch, yaw, roll);
        IGCS_UpdateData(px, py, pz, pitch, yaw, roll, g_CamFOV,
                        g_FreeCamActive);
      }
    } else if (IGCS_IsConnected()) {
      // Single zero-state push when in Sequence mode so ReShade sees the
      // camera tools as inactive
      IGCS_UpdateData(0, 0, 0, 0, 0, 0, g_CamFOV, false);
    }

    WAIT(0);
  }
}

void ScriptMain() {
  srand(GetTickCount());
  main();
}
