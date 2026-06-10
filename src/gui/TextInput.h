/*
        TextInput — a live text-entry buffer for the CommandPalette search box.

        ScriptHookV menus can't render the game's on-screen keyboard inline, so
        for a "type as you go" search box we capture raw keystrokes ourselves.
        ScriptHookV already delivers keystrokes through the keyboard handler you
        register in DllMain (see keyboard.cpp / keyboardHandlerRegister); this
        class turns those into an editable string with a caret.

        Wiring (once, in your keyboard handler):

            // keyboard.cpp -> OnKeyboardMessage(...)
            gtam::TextInput::FeedGlobal(key, scanCode, isExtended, isUpNow,
                                        isWithAlt);

        FeedGlobal forwards to whichever TextInput is currently focused (the
        palette focuses its buffer while open), and is a no-op when none is.
        Only this one line is needed in the host; everything else is internal.
*/

#pragma once

#include <string>

namespace gtam {

class TextInput {
public:
  // Focus / blur this buffer as the global keystroke sink. While focused, the
  // keyboard handler routes printable keys + editing keys here.
  void SetFocused(bool on);
  bool IsFocused() const;

  const std::string &Text() const { return text_; }
  void SetText(const std::string &t);
  void Clear();
  int  Caret() const { return caret_; }

  // True for one query if the text changed since the last call — lets the
  // palette re-run its search only when needed.
  bool ConsumeDirty();

  // Caret blink phase (0/1), wall-clock based; for drawing the caret bar.
  bool CaretVisible() const;

  // --- Keyboard routing ---
  // Called from the host's ScriptHookV keyboard handler for every key event.
  // Routes to the focused instance (if any). Safe to call always.
  static void FeedGlobal(unsigned long vkey, unsigned char scanCode,
                         int isExtended, int isUpNow, int isWithAlt);

private:
  void onKeyDown(unsigned long vkey, unsigned char scanCode, int isExtended,
                 int isWithAlt);
  void insertChar(char c);

  std::string text_;
  int  caret_ = 0;
  bool dirty_ = false;
};

} // namespace gtam
