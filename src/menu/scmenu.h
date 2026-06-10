/*
        Simple Camera — new GUI menu (built on the gtam NativeMenu framework).

        This is the reworked, modern menu for Free Camera mode. It runs in
        parallel with the classic NativeTrainer-style menu during the migration:
        the classic menu stays on F5; this one opens on F11 so both can be
        compared in-game. Once approved, F5 is repointed here and the classic
        menu/menu.cpp can be retired.

        Frame-driven (non-blocking): the host calls SCMenu_Update() every frame
        from the main loop, and SCMenu_Toggle() on the hotkey.
*/

#pragma once

// Menu appearance (customizable in the Appearance page, persisted to
// SimpleCamera.ini by Load/SaveSettings). Loaded before SCMenu_Init applies them.
extern float g_MenuPosX, g_MenuPosY, g_MenuScale;
extern int g_MenuAccentR, g_MenuAccentG, g_MenuAccentB;      // chrome accent
extern int g_MenuBgR, g_MenuBgG, g_MenuBgB;                  // panel background
extern int g_MenuTextR, g_MenuTextG, g_MenuTextB;            // normal text
extern int g_MenuSelR, g_MenuSelG, g_MenuSelB;               // selected-row fill
extern int g_MenuSelTextR, g_MenuSelTextG, g_MenuSelTextB;   // selected-row text

// Build the menu tree once. Call before the main loop (after LoadSettings).
void SCMenu_Init();

// Open/close the menu.
void SCMenu_Toggle();
bool SCMenu_IsOpen();

// Per-frame: draws + handles input while open. Drives g_MenuOpen so the free
// camera suppresses input the menu also consumes. No-op while closed.
void SCMenu_Update();
