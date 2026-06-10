/*
        NativeMenu вЂ” integration example (reference only).

        This file is NOT compiled into FreeCameraPlugin. It shows, end to end,
        how another ScriptHookV ASI mod wires up the reusable menu base.

        To use the menu in your own mod:
          1. Add gui/NativeMenu.h + gui/NativeMenu.cpp to your project.
          2. Build your menus once (statics or members).
          3. Call controller.Update() every frame, and Toggle() on your hotkey.

        Everything below is a working pattern you can copy.
*/

#include "NativeMenu.h"
#include "TextInput.h" // optional: live text-entry buffer (see section 5)

#ifndef NOMINMAX
#define NOMINMAX // keep <windows.h> min/max macros from breaking std::min/max
#endif

// Your mod's natives (adjust the path to wherever your SDK lives).
#include "main.h"
#include "natives.h"

#include <windows.h>

// ------------------------------------------------------------
//  1. The variables your menu drives. These are *your* mod's state вЂ” the menu
//     only reads/writes through the pointers you bind.
// ------------------------------------------------------------
static bool  g_GodMode = false;
static bool  g_InfiniteAmmo = true;
static float g_PlayerScale = 1.0f;
static int   g_WantedLevel = 0;
static int   g_TimeOfDay = 2; // index into the list below

static const char *kTimeNames[] = {"Dawn", "Morning", "Noon", "Evening", "Night"};

// ------------------------------------------------------------
//  2. The menu objects. Keep them alive for the program's lifetime (statics).
//     A submenu is just another Menu referenced by AddSubmenu.
// ------------------------------------------------------------
static gtam::Menu g_Main("MY MOD", "Main Menu");
static gtam::Menu g_PlayerMenu("MY MOD", "Player Options");
static gtam::Menu g_WorldMenu("MY MOD", "World Options");

static gtam::MenuController g_Menu;

// ------------------------------------------------------------
//  3. Build the menus once at startup.
// ------------------------------------------------------------
static void BuildMenus() {
  // --- Theme: start from the GTA-Online preset, then customise anything ---
  gtam::MenuTheme theme = gtam::MenuTheme::GtaOnline();
  theme.accent = gtam::Color(120, 200, 90, 255); // green accent instead of yellow
  theme.maxVisibleRows = 10;
  g_Menu.SetTheme(theme);

  // --- Layout / resize ---
  g_Menu.SetPosition(0.02f, 0.07f); // top-left anchor (screen fractions)
  g_Menu.SetScale(1.0f);            // overall size multiplier
  // Hold LEFT-CTRL to move (arrows) / scale ( - = ) / resize width ( [ ] ) /
  // reset ( 0 ) the menu live, in-game.
  g_Menu.EnableInteractiveResize(true, VK_LCONTROL);

  // --- Main page ---
  g_Main.AddSubmenu("Player", &g_PlayerMenu, "Health, ammo, size.");
  g_Main.AddSubmenu("World", &g_WorldMenu, "Time, wanted level.");
  g_Main.AddButton("Repair Vehicle",
                   [] {
                     Vehicle v = PED::GET_VEHICLE_PED_IS_IN(
                         PLAYER::PLAYER_PED_ID(), FALSE);
                     if (v) VEHICLE::SET_VEHICLE_FIXED(v);
                   },
                   "Fully repairs the car you're in.");
  // A button with a live right-hand readout via valueGetter.
  g_Main.AddButton("Coordinates", nullptr, "Your current position.")
      .valueGetter = [] {
    Vector3 p = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), TRUE);
    char buf[64];
    sprintf_s(buf, "%.0f, %.0f", p.x, p.y);
    return std::string(buf);
  };

  // --- Player page ---
  g_PlayerMenu.AddToggle("God Mode", &g_GodMode,
                         [](bool on) {
                           PLAYER::SET_PLAYER_INVINCIBLE(PLAYER::PLAYER_ID(), on);
                         },
                         "Take no damage.");
  g_PlayerMenu.AddToggle("Infinite Ammo", &g_InfiniteAmmo, nullptr,
                         "Never reload.");
  g_PlayerMenu.AddFloat("Player Scale", &g_PlayerScale, 0.25f, 3.0f, 0.05f, 2,
                        nullptr, "Resize your character.");

  // --- World page ---
  g_WorldMenu.AddInt("Wanted Level", &g_WantedLevel, 0, 5, 1,
                     [](int lvl) {
                       PLAYER::SET_PLAYER_WANTED_LEVEL(PLAYER::PLAYER_ID(), lvl,
                                                       FALSE);
                       PLAYER::SET_PLAYER_WANTED_LEVEL_NOW(PLAYER::PLAYER_ID(),
                                                           FALSE);
                     },
                     "Stars on your head.");
  g_WorldMenu.AddList(
      "Time of Day", &g_TimeOfDay,
      {"Dawn", "Morning", "Noon", "Evening", "Night"},
      [](int idx) {
        static const int hours[] = {6, 9, 12, 18, 23};
        TIME::SET_CLOCK_TIME(hours[idx], 0, 0);
      },
      "Jump the game clock.");
  g_WorldMenu.AddSeparator("Danger Zone");
  g_WorldMenu.AddButton("Wanted: instant 5 stars", [] {
    PLAYER::SET_PLAYER_WANTED_LEVEL(PLAYER::PLAYER_ID(), 5, FALSE);
    PLAYER::SET_PLAYER_WANTED_LEVEL_NOW(PLAYER::PLAYER_ID(), FALSE);
  });

  // --- An optional left-hand game icon on a row (built-in txd, no assets) ---
  g_Main.AddButton("Heal Player", [] {
    Ped p = PLAYER::PLAYER_PED_ID();
    ENTITY::SET_ENTITY_HEALTH(p, 200);
  }).iconDict = "commonmenu";
  // (set both fields; iconName picks the sprite within the dict)
  g_Main.items.back().iconName = "mp_specitem_health";
}

// ------------------------------------------------------------
//  4. Main loop. Toggle on a hotkey; Update() every frame.
// ------------------------------------------------------------
void ExampleScriptMain() {
  BuildMenus();

  while (true) {
    // F4 toggles the menu (use your own edge-detected key check).
    if (GetAsyncKeyState(VK_F4) & 1) g_Menu.Toggle(&g_Main);

    // Drives input + drawing while open. Returns true if visible so you can
    // skip your own per-frame logic that frame.
    bool visible = g_Menu.Update();
    if (!visible) {
      // ... your normal per-frame mod logic here ...
    }

    WAIT(0);
  }
}

// ------------------------------------------------------------
//  5. (Optional) gtam::TextInput - a standalone live text-entry buffer you can
//     use for an in-menu rename / search field. Feed it from your ScriptHookV
//     keyboard handler (registered via keyboardHandlerRegister in DllMain). It
//     only acts while a TextInput is focused, so this one line is safe to always
//     run:
//
//     void OnKeyboardMessage(DWORD key, WORD repeats, BYTE scanCode,
//                            BOOL isExtended, BOOL isWithAlt,
//                            BOOL wasDownBefore, BOOL isUpNow) {
//         gtam::TextInput::FeedGlobal(key, scanCode, isExtended, isUpNow,
//                                     isWithAlt);
//         // ... your existing key handling ...
//     }
// ------------------------------------------------------------
