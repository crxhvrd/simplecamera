/*
        GTA V Free Camera / Photo Mode Plugin
        In-Game Menu System Header
*/

#pragma once

#include "external\scripthook_sdk\inc\enums.h"
#include "external\scripthook_sdk\inc\main.h"
#include "external\scripthook_sdk\inc\natives.h"
#include "external\scripthook_sdk\inc\types.h"

#include <string>

// Settings
void LoadSettings();

// Menu toggle key
bool IsMenuTogglePressed();

// Menu drawing helpers
void DrawMenuLine(std::string caption, std::string value, float lineWidth,
                  float lineHeight, float lineTop, float lineLeft,
                  float textLeft, bool active, bool title,
                  bool rescaleText = true);

void DrawMenuValue(std::string caption, std::string value, float lineWidth,
                   float lineHeight, float lineTop, float lineLeft,
                   float textLeft, bool active);

void MenuBeep();
void SetStatusText(std::string str, DWORD time = 2500);
void UpdateStatusText();

// Process the main configuration menu (blocks until closed)
void ProcessConfigMenu();
