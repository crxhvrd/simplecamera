/*
        GTA V Free Camera / Photo Mode Plugin
        Main Script Loop

        Entry point for the ScriptHookV runtime. Runs the main loop
        that checks for menu activation and updates the free camera each frame.
*/

#include "script.h"
#include "camera.h"
#include "igcs_bridge.h"
#include "keyboard.h"
#include "menu.h"


#pragma warning(disable : 4244 4305)

// ============================================================
//  Main Loop
// ============================================================

void main() {
  // Load INI settings
  LoadSettings();

  // Reset defaults
  g_FreeCamActive = false;
  g_CamSpeed = 1.0f;
  g_CamSensitivity = 1.0f;
  g_CamFOV = 50.0f;
  g_CamRoll = 0.0f;
  g_ZoomSpeed = 1.0f;
  g_DoFEnabled = false;
  g_DoFFocusDist = 10.0f;
  g_DoFMaxNearInFocus = 0.5f;
  g_TimePaused = false;
  g_TimeHour = 12;
  g_TimeMinute = 0;
  g_Weather1Index = 0;
  g_Weather2Index = 1;
  g_WeatherBlend = 0.0f;

  while (true) {
    // Check for menu toggle (F5)
    if (IsMenuTogglePressed()) {
      MenuBeep();
      ProcessConfigMenu();
    }

    // Update camera every frame if active
    if (g_FreeCamActive) {
      UpdateFreeCamera();
    }

    // Update time/weather overrides
    UpdateTimeWeather();

    // Update on-screen status text
    UpdateStatusText();

    // Global effects (freeze world, info overlay)
    UpdateGlobalEffects();

    // Update IGCS shared buffer with current camera state
    IGCS_TryConnect();
    if (IGCS_IsConnected()) {
      float px, py, pz, pitch, yaw, roll;
      GetCameraState(px, py, pz, pitch, yaw, roll);
      IGCS_UpdateData(px, py, pz, pitch, yaw, roll, g_CamFOV, g_FreeCamActive);
    }

    WAIT(0);
  }
}

void ScriptMain() {
  srand(GetTickCount());
  main();
}
