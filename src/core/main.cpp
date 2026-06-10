/*
        GTA V Free Camera / Photo Mode Plugin
        DLL Entry Point

        Registers the script and keyboard handler with ScriptHookV.
*/

#include "external\scripthook_sdk\inc\main.h"
#include "keyboard.h"
#include "script.h"

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved) {
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    scriptRegister(hInstance, ScriptMain);
    keyboardHandlerRegister(OnKeyboardMessage);
    break;
  case DLL_PROCESS_DETACH:
    scriptUnregister(hInstance);
    keyboardHandlerUnregister(OnKeyboardMessage);
    break;
  }
  return TRUE;
}
