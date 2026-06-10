/*
        TextInput — implementation. See TextInput.h.

        Key translation uses the Win32 keyboard layout (ToUnicode) so it
        respects Shift/Caps and the user's layout, while editing keys
        (Backspace/Delete/arrows/Home/End) are handled explicitly. Enter, Tab
        and Escape are intentionally NOT consumed here — the palette handles
        those through the game's frontend controls in its per-frame loop.
*/

#include "TextInput.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

namespace gtam {

// The single focused buffer that receives keystrokes. Set by SetFocused.
static TextInput *s_focused = nullptr;

void TextInput::SetFocused(bool on) {
  if (on) {
    s_focused = this;
  } else if (s_focused == this) {
    s_focused = nullptr;
  }
}

bool TextInput::IsFocused() const { return s_focused == this; }

void TextInput::SetText(const std::string &t) {
  text_ = t;
  caret_ = (int)text_.size();
  dirty_ = true;
}

void TextInput::Clear() {
  if (!text_.empty()) dirty_ = true;
  text_.clear();
  caret_ = 0;
}

bool TextInput::ConsumeDirty() {
  bool d = dirty_;
  dirty_ = false;
  return d;
}

bool TextInput::CaretVisible() const {
  // ~530ms blink period.
  return ((GetTickCount() / 530) & 1ul) == 0;
}

void TextInput::insertChar(char c) {
  caret_ = std::max(0, std::min((int)text_.size(), caret_));
  text_.insert(text_.begin() + caret_, c);
  caret_++;
  dirty_ = true;
}

void TextInput::onKeyDown(unsigned long vkey, unsigned char scanCode,
                          int isExtended, int isWithAlt) {
  // Editing / navigation keys first.
  switch (vkey) {
  case VK_BACK:
    if (caret_ > 0) {
      text_.erase(text_.begin() + (caret_ - 1));
      caret_--;
      dirty_ = true;
    }
    return;
  case VK_DELETE:
    if (caret_ < (int)text_.size()) {
      text_.erase(text_.begin() + caret_);
      dirty_ = true;
    }
    return;
  case VK_LEFT:
    if (caret_ > 0) caret_--;
    return;
  case VK_RIGHT:
    if (caret_ < (int)text_.size()) caret_++;
    return;
  case VK_HOME:
    caret_ = 0;
    return;
  case VK_END:
    caret_ = (int)text_.size();
    return;
  // Don't consume these — the palette reads them via frontend controls.
  case VK_RETURN:
  case VK_ESCAPE:
  case VK_TAB:
  case VK_UP:
  case VK_DOWN:
    return;
  default:
    break;
  }

  // Ignore Alt/Ctrl chords (e.g. Alt+Tab) — they aren't text.
  if (isWithAlt) return;
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000) return;

  // Translate the virtual key to a character using the active layout, honoring
  // Shift/Caps. ToUnicode reads live modifier state from the keyboard buffer.
  BYTE kb[256];
  if (!GetKeyboardState(kb)) return;
  WCHAR out[4] = {0};
  int n = ToUnicode((UINT)vkey, scanCode, kb, out, 4, 0);
  if (n == 1) {
    wchar_t wc = out[0];
    if (wc >= 32 && wc < 127) // printable ASCII; the GTA fonts are ASCII-only
      insertChar((char)wc);
  }
}

void TextInput::FeedGlobal(unsigned long vkey, unsigned char scanCode,
                           int isExtended, int isUpNow, int isWithAlt) {
  if (!s_focused) return;
  if (isUpNow) return; // act on key-down only
  s_focused->onKeyDown(vkey, scanCode, isExtended, isWithAlt);
}

} // namespace gtam
