/*
        NativeMenu — a reusable GTA V (ScriptHookV) in-game GUI menu base.

        Drop-in menu framework for ASI mods. Renders a GTA-Online / NativeUI
        style menu using only ScriptHookV natives (no DirectX hooks, no
        external UI libs), so it works inside any script that already has a
        per-frame loop.

        Design goals
          * Reusable    — zero coupling to any one mod. You build menus from
                          items that bind to your own variables / callbacks.
          * Customizable— every colour, font, scale and layout metric lives in
                          MenuTheme. Swap the whole look in one assignment.
          * GTA 5 style — title banner, accent subtitle bar, highlighted row,
                          right-hand values, scrollbar, description panel.
          * Resizeable  — scale, width and screen position are live-adjustable
                          at runtime (programmatically or via built-in hotkeys),
                          and resolution-independent (designed on a 1080p grid).

        Usage (host side, once):
            static gtam::Menu  main("MY MOD", "Main Menu");
            static gtam::MenuController menu;
            main.AddToggle("God Mode", &g_God);
            main.AddFloat ("Speed", &g_Speed, 0.f, 10.f, 0.25f, 2);
            menu.Open(&main);

        Then every frame:
            if (justPressedF5) menu.Toggle(&main);
            menu.Update();          // draws + handles input when open

        Only NativeMenu.cpp pulls in the ScriptHookV headers; this header is
        pure C++/STL so it can be included anywhere.
*/

#pragma once

#include "TextInput.h" // inline "type an exact value" editing on Float/Int rows

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gtam {

// ============================================================
//  Theme
// ============================================================

struct Color {
  int r = 255, g = 255, b = 255, a = 255;
  Color() = default;
  Color(int r_, int g_, int b_, int a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
};

// Every visual attribute of the menu. Build one, tweak it, hand it to a
// MenuController. The layout metrics are expressed in "design pixels" on a
// 1920x1080 grid — they are converted to resolution-independent screen
// fractions at draw time and multiplied by the controller's live scale, so the
// menu looks identical at 1080p, 1440p or 4K and resizes cleanly.
struct MenuTheme {
  // --- Title banner ---
  Color titleBg{0, 0, 0, 255};
  Color titleText{255, 255, 255, 255};
  int   titleFont = 1;        // 1 = House Script (cursive GTA title font)
  float titleScale = 0.95f;
  bool  titleCentered = true;

  // --- Subtitle / accent bar (also hosts the "x / y" counter) ---
  Color subtitleBg{0, 0, 0, 255};
  Color subtitleText{255, 255, 255, 255};
  Color accent{245, 200, 60, 255}; // GTA-yellow accent used for the bar + chrome
  bool  accentSubtitleBar = true;  // paint the subtitle bar with `accent`
  int   subtitleFont = 0;
  float subtitleScale = 0.34f;

  // --- Rows ---
  Color rowBg{0, 0, 0, 200};            // normal row background (translucent)
  Color rowText{255, 255, 255, 255};
  Color rowSelectedBg{245, 245, 245, 255}; // highlighted row background
  Color rowSelectedText{10, 10, 10, 255};  // highlighted row text
  // Optional cyan-style outline around the selected row (the command-palette
  // look). Set rowSelectedOutlinePx = 0 to disable (default).
  Color rowSelectedOutline{60, 200, 220, 255};
  float rowSelectedOutlinePx = 0.0f;
  Color disabledText{155, 155, 155, 255};
  Color separatorText{200, 200, 200, 255};
  int   rowFont = 4;          // 4 = Chalet Comprime (standard GTA menu font)
  float rowTextScale = 0.345f;

  // Selected-row accent bar drawn flush on the left edge (modern look). Set
  // accentBarWidthPx = 0 to disable.
  float accentBarWidthPx = 4.0f;
  // Icon column. Rows with an icon reserve this much width on the left; the
  // caption indents past it. Set iconColumnPx = 0 to disable icons globally.
  float iconColumnPx = 30.0f;
  float iconSizePx = 22.0f;

  // Value "pill": adjustable rows (float/int/list) draw their value inside a
  // rounded-less but tinted capsule on selected rows so it reads as editable.
  bool  valuePill = true;
  Color valuePillBg{0, 0, 0, 90};
  Color valuePillBgSelected{0, 0, 0, 60};

  // --- Description panel (under the list, shows selected item's help text) ---
  bool  showDescription = true;
  Color descBg{0, 0, 0, 230};
  Color descText{255, 255, 255, 255};
  float descScale = 0.30f;

  // --- Scrollbar (shown when the list is taller than the visible window) ---
  bool  showScrollbar = true;
  Color scrollTrack{0, 0, 0, 120};
  Color scrollThumb{245, 200, 60, 255};

  // --- Footer (controls hint on the left + an app status note on the right) ---
  bool  showFooter = true;
  Color footerBg{0, 0, 0, 235};
  Color footerText{165, 172, 182, 255};
  float footerHeightPx = 30.0f;

  // --- Toggle checkbox (drawn, no sprite dependency) ---
  Color checkboxBorder{120, 120, 120, 255};
  Color checkboxFill{245, 200, 60, 255};

  // --- Layout metrics (design px @ 1080p) ---
  float widthPx = 432.0f;     // menu width
  float rowHeightPx = 38.0f;
  float titleHeightPx = 76.0f;
  float subtitleHeightPx = 38.0f;
  float descHeightPx = 44.0f;
  float scrollbarWidthPx = 6.0f;

  // --- Behaviour ---
  bool  playSounds = true;    // frontend nav / select / back beeps
  bool  wrapNavigation = true; // up past top jumps to bottom and vice-versa
  int   maxVisibleRows = 12;  // rows shown before the list scrolls

  // Preset factories.
  static MenuTheme GtaOnline();  // default: black rows, yellow accent
  static MenuTheme Dark();       // muted dark/blue
  static MenuTheme Light();      // pale panel, dark text
  static MenuTheme Spotlight();  // command-palette look: dark panel, cyan accent,
                                 // outlined selected row
};

// ============================================================
//  Items
// ============================================================

enum class ItemType { Button, Toggle, Float, Int, List, Label, Separator, Submenu };

class Menu; // fwd

// A single row. Construct these through Menu::Add* helpers rather than by hand.
// All callbacks are optional. Bound pointers (boolPtr / floatPtr / ...) are
// owned by the host; the menu only reads and writes through them.
struct MenuItem {
  ItemType type = ItemType::Button;
  std::string text;
  std::string description;
  bool enabled = true;

  // Optional left-hand icon drawn from a streamed game texture dictionary
  // (e.g. dict "commonmenu", name "shop_box_blip"). Leave iconName empty for no
  // icon. Only built-in game txds are used — no custom assets required.
  std::string iconDict;
  std::string iconName;
  Color iconTint{255, 255, 255, 255};

  // A static badge drawn on the right of a Button (e.g. a hotkey or status).
  std::string rightLabel;
  // Optional dynamic right-text provider; overrides rightLabel when set. Lets a
  // row show live data ("12 entities", a coordinate, a clock) without you
  // having to poke a value pointer every frame.
  std::function<std::string()> valueGetter;

  // Toggle
  bool *boolPtr = nullptr;

  // Float slider
  float *floatPtr = nullptr;
  float fMin = 0.f, fMax = 1.f, fStep = 0.1f;
  int   fDecimals = 2;

  // Int slider
  int *intPtr = nullptr;
  int iMin = 0, iMax = 100, iStep = 1;

  // List / choice
  int *listIndexPtr = nullptr;
  std::vector<std::string> listOptions;

  // Submenu target (host-owned)
  Menu *submenu = nullptr;

  // Callbacks. onActivate fires on Select/Enter (Button/Toggle/List/Submenu).
  // onChange fires whenever the bound value changes (slider step, list cycle,
  // toggle flip). The argument is the item itself so one handler can serve many
  // rows; convenience typed lambdas are provided by the Add* helpers.
  std::function<void(MenuItem &)> onActivate;
  std::function<void(MenuItem &)> onChange;
};

// ============================================================
//  Menu
// ============================================================

// A page of items with a title and subtitle. Host code owns Menu instances
// (commonly as statics or members) and keeps them alive while open. Submenus
// are just other Menu objects referenced by a Submenu item.
class Menu {
public:
  Menu() = default;
  Menu(std::string title, std::string subtitle = "")
      : title(std::move(title)), subtitle(std::move(subtitle)) {}

  // --- Builders. Each returns the created item by reference so you can keep
  //     tweaking it (set .description, .enabled, .rightLabel, ...). ---

  MenuItem &AddButton(const std::string &text,
                      std::function<void()> onClick = nullptr,
                      const std::string &desc = "");

  MenuItem &AddToggle(const std::string &text, bool *value,
                      std::function<void(bool)> onChange = nullptr,
                      const std::string &desc = "");

  MenuItem &AddFloat(const std::string &text, float *value, float mn, float mx,
                     float step, int decimals = 2,
                     std::function<void(float)> onChange = nullptr,
                     const std::string &desc = "");

  MenuItem &AddInt(const std::string &text, int *value, int mn, int mx,
                   int step = 1, std::function<void(int)> onChange = nullptr,
                   const std::string &desc = "");

  // `options` are the display strings; `index` is the host-owned selected index.
  MenuItem &AddList(const std::string &text, int *index,
                    std::vector<std::string> options,
                    std::function<void(int)> onChange = nullptr,
                    const std::string &desc = "");

  MenuItem &AddSubmenu(const std::string &text, Menu *sub,
                       const std::string &desc = "");

  // A non-selectable caption row (skipped by navigation).
  MenuItem &AddLabel(const std::string &text);
  // A non-selectable divider; optional centred caption.
  MenuItem &AddSeparator(const std::string &caption = "");

  // Push a fully-formed item (advanced use).
  MenuItem &Add(MenuItem item);

  void Clear();
  size_t Count() const { return items.size(); }

  std::string title;
  std::string subtitle;
  std::vector<MenuItem> items;

  // Per-menu theme override. When set, this page uses it instead of the
  // controller's theme — handy for a "danger" red submenu, etc.
  std::shared_ptr<MenuTheme> themeOverride;

  // Lifecycle hooks. onPush fires when this menu is pushed onto the controller
  // stack (entered); onPop fires when it is popped off (left, via Back or
  // Close). Use them to enter/leave a mode, lazily (re)build a dynamic list, or
  // sync state. Sub-navigation into a child menu does NOT pop this one, so a
  // root that opens children keeps its onPush state until fully left.
  std::function<void()> onPush;
  std::function<void()> onPop;

  // Runtime cursor state (managed by the controller).
  int selected = 0;
  int scrollOffset = 0;
};

// ============================================================
//  Controller
// ============================================================

// Drives one menu stack: input, navigation, value editing, drawing and live
// resize. Create one per mod and call Update() every frame. It maintains a
// breadcrumb stack so entering a Submenu item pushes, and Back pops.
class MenuController {
public:
  MenuController();

  // --- Theme ---
  void SetTheme(const MenuTheme &t) { theme_ = t; }
  MenuTheme &Theme() { return theme_; }
  const MenuTheme &Theme() const { return theme_; }

  // --- Layout / resize (all live) ---
  // Top-left anchor as a fraction of the screen (0..1). Default {0.02, 0.08}.
  void SetPosition(float xFrac, float yFrac);
  void GetPosition(float &xFrac, float &yFrac) const { xFrac = posX_; yFrac = posY_; }
  // Overall size multiplier applied to width + heights + text. Clamped to a
  // sane range. Default 1.0.
  void SetScale(float s);
  float Scale() const { return scale_; }
  // Override the menu width in design px (otherwise theme.widthPx is used).
  void SetWidth(float widthPx);
  float Width() const;

  // --- Interactive layout editing ---
  // When enabled, holding `modifierVK` (default Left-Ctrl) turns the arrow keys
  // into move controls, [ / ] into width, - / = into scale, and 0 resets.
  // Navigation is suppressed while the modifier is held so the two never fight.
  void EnableInteractiveResize(bool on, int modifierVK = 0xA2 /*VK_LCONTROL*/);

  // --- Open / close ---
  void Open(Menu *root);      // open at `root` (resets the stack)
  void Close();               // close everything
  void Toggle(Menu *root);    // open `root` if closed, else close
  bool IsOpen() const { return !stack_.empty(); }
  Menu *Current() const { return stack_.empty() ? nullptr : stack_.back(); }

  // Programmatic navigation helpers (rarely needed by hosts).
  void Push(Menu *m);
  void Back();

  // --- Per-frame entry point ---
  // Draws the menu and processes input when open. Returns true if the menu is
  // currently visible (so the host can suppress its own input that frame).
  // Pass the real frame delta (seconds) if you have it for frame-rate-independent
  // key repeat; otherwise it uses a wall-clock timer.
  bool Update();

  // Disable the game controls the menu reuses (movement, attack, phone,
  // character wheel...) while open so the player/world don't react to nav
  // input. On by default.
  void SetDisableGameControls(bool on) { disableControls_ = on; }

  // Optional right-aligned note shown in the footer (e.g. the active mode).
  // Set it each frame; pass "" to clear.
  void SetFooterText(const std::string &t) { footerNote_ = t; }

private:
  // input
  void readInput();
  void applyNavigation();
  void editCurrentItem(int dir); // dir: -1 left, +1 right
  void activateCurrentItem();
  int  firstSelectable(int from, int dir) const;
  void ensureVisible();

  // inline value typing (Float/Int rows): Enter starts, Enter commits, Esc cancels
  void beginValueEdit(int index);
  void updateValueEdit();
  void commitValueEdit();
  void cancelValueEdit();
  bool isEditingValue() const { return editMenu_ != nullptr; }

  // resize
  void handleInteractiveResize();

  // draw
  void draw();
  void playSound(const char *name);

  MenuTheme theme_;
  std::vector<Menu *> stack_;

  float posX_ = 0.02f, posY_ = 0.07f;
  float scale_ = 1.0f;
  float widthOverridePx_ = -1.0f; // <0 => use theme width

  bool disableControls_ = true;
  std::string footerNote_;

  // inline value-edit state
  TextInput valueInput_;
  Menu *editMenu_ = nullptr; // the menu whose row is being typed into (null = not editing)
  int editIndex_ = -1;
  bool prevEditEsc_ = false;

  // interactive resize
  bool resizeEnabled_ = false;
  int  resizeModifierVK_ = 0xA2;

  // edge/repeat state
  struct Btn { bool down = false, pressed = false; unsigned long nextRepeat = 0; };
  static bool Repeat(Btn &b, bool downNow, unsigned long now,
                     unsigned long initialMs, unsigned long intervalMs);
  Btn up_, down_, left_, right_, select_, back_;
  unsigned long holdStart_ = 0; // for slider acceleration
  bool wasAdjusting_ = false;
};

} // namespace gtam
