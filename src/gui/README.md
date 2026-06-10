# NativeMenu — reusable GTA V in-game menu base

A drop-in, GTA-Online / NativeUI-style menu framework for **ScriptHookV ASI
mods**. It renders entirely with ScriptHookV UI natives (no DirectX hooks, no
ImGui, no external UI libs), so it works inside any script that already has a
per-frame loop.

It was factored out of the FreeCameraPlugin menu so other CoreFX mods can share
one consistent, good-looking menu instead of each re-implementing
`DRAW_RECT`/`_DRAW_TEXT` glue.

## Why use it

| Goal | How |
|------|-----|
| **Reusable** | Zero coupling to any one mod. You bind rows to *your* variables and callbacks. Copy two files into any project. |
| **Customizable** | Every colour, font, scale and layout metric lives in `MenuTheme`. Swap the whole look in one assignment, or override per submenu. |
| **GTA 5 style** | Title banner, accent subtitle bar with `sel / total` counter, highlighted row, right-hand values, drawn checkboxes, scrollbar, description panel, frontend nav sounds. |
| **Resizeable** | `scale`, `width` and screen `position` are live-adjustable — programmatically or with built-in in-game hotkeys. Resolution-independent (authored on a 1080p grid). |

## Files

- `NativeMenu.h` / `NativeMenu.cpp` — the menu API + implementation (controller, menus, items, theme).
- `MenuDraw.h` / `MenuDraw.cpp` — shared low-level draw helpers used by the menu.
- `TextInput.h` / `TextInput.cpp` — optional standalone live text-entry buffer (see below).
- `Example.cpp` — a full reference wiring (not compiled into the plugin).

Add `NativeMenu.cpp` + `MenuDraw.cpp` (and `TextInput.cpp` if you use it) to your
`.vcxproj`. The GUI sources use **bare** SDK includes (`#include "natives.h"`),
so they're location-independent — just put the ScriptHookV `inc` folder on your
`AdditionalIncludeDirectories`.

## Quick start

```cpp
#include "gui/NativeMenu.h"

static bool  g_God = false;
static float g_Speed = 1.0f;

static gtam::Menu           main("MY MOD", "Main Menu");
static gtam::MenuController menu;

void ScriptMain() {
    main.AddToggle("God Mode", &g_God);
    main.AddFloat ("Speed", &g_Speed, 0.f, 10.f, 0.25f, /*decimals*/2);
    main.AddButton("Say Hi", [] { /* ... */ });

    while (true) {
        if (justPressedF5())        // your own edge-detected key
            menu.Toggle(&main);
        menu.Update();              // draws + handles input while open
        WAIT(0);
    }
}
```

## Item types

Build rows through `Menu::Add*`. Each returns the created `MenuItem&` so you can
keep tweaking it (`.description`, `.enabled`, `.rightLabel`, `.valueGetter`).

| Builder | Row behaviour |
|---------|---------------|
| `AddButton(text, onClick, desc)` | Fires `onClick` on Select. Optional right-side badge via `.rightLabel` or live `.valueGetter`. |
| `AddToggle(text, &bool, onChange, desc)` | Drawn checkbox. Select or Left/Right flips it. |
| `AddFloat(text, &float, min, max, step, decimals, onChange, desc)` | Left/Right adjusts; **hold to accelerate** (bigger + faster steps). |
| `AddInt(text, &int, min, max, step, onChange, desc)` | Integer slider, same accel. |
| `AddList(text, &index, {"A","B",...}, onChange, desc)` | Cycles options with Left/Right (and Select). |
| `AddSubmenu(text, &otherMenu, desc)` | Select pushes into another `Menu`; Back pops. |
| `AddLabel(text)` | Non-selectable caption row. |
| `AddSeparator(caption)` | Non-selectable divider (optional centred caption). |

`onChange` fires whenever a bound value changes; `onActivate`/`onClick` fires on
Select. Both are optional.

## Controls

Input is read through the game's **frontend controls**, which map *both*
keyboard and controller:

| Action | Keyboard | Controller |
|--------|----------|------------|
| Navigate | Arrow Up/Down | D-pad Up/Down |
| Adjust / cycle | Arrow Left/Right | D-pad Left/Right |
| Select | Enter | A |
| Back / close | Backspace | B |

While the menu is open it disables only the controls it reuses (frontend nav,
phone, weapon/character wheels) so the world doesn't react to nav input —
movement/look stay enabled, so opening the menu on foot doesn't freeze the
player. Toggle all of it with `SetDisableGameControls(false)`.

## Resizing

**Programmatic** (call anytime):

```cpp
menu.SetScale(1.25f);          // overall size multiplier (0.5 .. 2.5)
menu.SetWidth(480.f);          // width in design px (1080p grid)
menu.SetPosition(0.70f, 0.10f); // top-left anchor, screen fractions
```

**Interactive** (in-game), enable once:

```cpp
menu.EnableInteractiveResize(true, VK_LCONTROL);
```

Then, while the menu is open, **hold Left-Ctrl** and:

- **Arrow keys** — move the menu
- **`-` / `=`** — shrink / grow scale
- **`[` / `]`** — narrower / wider
- **`0`** — reset to defaults

Persist the result yourself (e.g. to your INI) via `Scale()`, `Width()`,
`GetPosition()` and restore on load. The menu stays proportional and
resolution-independent at every size.

## Theming

```cpp
gtam::MenuTheme t = gtam::MenuTheme::Spotlight();  // or GtaOnline()/Dark()/Light()
t.accent          = gtam::Color(120, 200, 90);    // green accent
t.rowSelectedBg   = gtam::Color(120, 200, 90);
t.titleFont       = 1;        // 1 = House Script, 4 = Chalet (GTA menu font)
t.maxVisibleRows  = 10;
t.widthPx         = 460.f;
t.showDescription = true;
menu.SetTheme(t);
```

Presets: `GtaOnline()` (black rows, yellow accent), `Spotlight()` (dark panel +
cyan accent + outlined selected row — the command-palette skin), `Dark()`,
`Light()`.

Per-page override (e.g. a red "danger" submenu):

```cpp
auto danger = std::make_shared<gtam::MenuTheme>(gtam::MenuTheme::Dark());
danger->accent = gtam::Color(220, 60, 60);
dangerMenu.themeOverride = danger;
```

### Theme fields (highlights)

`titleBg/titleText/titleFont/titleScale`, `accent`, `subtitleBg/subtitleText`,
`rowBg/rowText`, `rowSelectedBg/rowSelectedText`, `disabledText`,
`descBg/descText`, `scrollTrack/scrollThumb`, `checkboxBorder/checkboxFill`,
plus layout metrics `widthPx/rowHeightPx/titleHeightPx/subtitleHeightPx/
descHeightPx`, and behaviour flags `playSounds/wrapNavigation/maxVisibleRows/
showDescription/showScrollbar`.

## Live text input (optional)

`gtam::TextInput` is a small **standalone** live text-entry buffer (caret +
layout-aware key handling) for things like an in-menu rename or search field. It
is independent of the menu — use it anywhere you need typed input.

Feed it from your ScriptHookV keyboard handler (the one you register with
`keyboardHandlerRegister` in `DllMain`). The forward only acts while a TextInput
is focused, so it's always safe to leave in:

```cpp
#include "gui/TextInput.h"

void OnKeyboardMessage(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended,
                       BOOL isWithAlt, BOOL wasDownBefore, BOOL isUpNow) {
    gtam::TextInput::FeedGlobal(key, scanCode, isExtended, isUpNow, isWithAlt);
    // ... your existing key handling ...
}
```

`TextInput` translates virtual keys through the active layout (`ToUnicode`, so
Shift/Caps and non-US layouts work) and handles Backspace/Delete/Home/End and
caret movement. Call `SetFocused(true)` while your field is active, then read
`Text()` / `Caret()` to draw it. Enter/Esc/Tab/arrows are left alone for your
own navigation.

## Icons (built-in game sprites)

Any `MenuItem` can show a left-hand icon drawn from a streamed
game texture dictionary — no custom assets:

```cpp
auto& row = menu.AddButton("Heal", []{ /*...*/ });
row.iconDict = "commonmenu";
row.iconName = "mp_specitem_health";
```

The framework streams the dict on demand (`REQUEST_STREAMED_TEXTURE_DICT`) and
reserves the icon column only when at least one row in the menu has an icon.
Useful built-in dicts: `commonmenu`, `commonmenutu`, `mpleaderboard`. Leave
`iconName` empty for a text-only row.

## Notes / limitations

- Fonts are ASCII-safe: toggles render as **drawn checkboxes** and adjustable
  rows use ASCII `< value >` chevrons, because the GTA menu font (Chalet) has no
  arrow/checkmark glyph coverage. No `commonmenu.ytd` sprite dependency.
- `Update()` is non-blocking — it draws one frame and returns. It does **not**
  spin its own `WAIT(0)` loop, so it composes with whatever else your script
  does each frame (unlike the older blocking FreeCameraPlugin menu).
- Host owns all `Menu` objects; keep them alive while open. Submenus are plain
  `Menu`s referenced by an `AddSubmenu` row.

See `Example.cpp` for a complete, copy-pasteable integration.
