/*
        MenuDraw вЂ” implementation. See MenuDraw.h. Only this TU includes the
        ScriptHookV natives for the GUI layer.
*/

#include "MenuDraw.h"

#ifndef NOMINMAX
#define NOMINMAX // <windows.h> min/max macros vs std::min/std::max
#endif

#include "main.h"
#include "natives.h"

#pragma warning(disable : 4244 4305)

namespace gtam {
namespace draw {

void RectTL(float x, float y, float w, float h, const Color &c) {
  // DRAW_RECT is centre-anchored; offset by half extent to anchor top-left.
  GRAPHICS::DRAW_RECT(x + w * 0.5f, y + h * 0.5f, w, h, c.r, c.g, c.b, c.a);
}

void RectOutline(float x, float y, float w, float h, const Color &c,
                 float thicknessPx) {
  float tx = PxX(thicknessPx), ty = PxY(thicknessPx);
  RectTL(x, y, w, ty, c);             // top
  RectTL(x, y + h - ty, w, ty, c);    // bottom
  RectTL(x, y, tx, h, c);             // left
  RectTL(x + w - tx, y, tx, h, c);    // right
}

void Text(const std::string &s, float x, float y, float scale, int font,
          const Color &c, Align align, float rightEdgeFrac) {
  UI::SET_TEXT_FONT(font);
  UI::SET_TEXT_SCALE(0.0f, scale);
  UI::SET_TEXT_COLOUR(c.r, c.g, c.b, c.a);
  UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
  UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
  UI::SET_TEXT_CENTRE(align == Align::Center ? 1 : 0);
  if (align == Align::Right) {
    UI::SET_TEXT_RIGHT_JUSTIFY(1);
    UI::SET_TEXT_WRAP(0.0f, rightEdgeFrac);
  }
  UI::_SET_TEXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)s.c_str());
  UI::_DRAW_TEXT(x, y);
  // These are sticky global text states; reset so the next call starts clean.
  UI::SET_TEXT_RIGHT_JUSTIFY(0);
  UI::SET_TEXT_WRAP(0.0f, 1.0f);
}

float TextWidth(const std::string &s, float scale, int font) {
  UI::SET_TEXT_FONT(font);
  UI::SET_TEXT_SCALE(0.0f, scale);
  UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
  UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
  UI::_SET_TEXT_ENTRY_FOR_WIDTH((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)s.c_str());
  return UI::_GET_TEXT_SCREEN_WIDTH(TRUE);
}

float TextLineHeight(float scale, int font) {
  // 0xDB88A37483346780 = _GET_TEXT_SCALE_HEIGHT(size, font) -> screen fraction.
  return invoke<float>(0xDB88A37483346780, scale, font);
}

int TextLineCount(const std::string &s, float scale, int font,
                  float wrapStartFrac, float wrapEndFrac) {
  UI::SET_TEXT_FONT(font);
  UI::SET_TEXT_SCALE(0.0f, scale);
  UI::SET_TEXT_WRAP(wrapStartFrac, wrapEndFrac);
  UI::_SET_TEXT_GXT_ENTRY((char *)"STRING");
  UI::_ADD_TEXT_COMPONENT_STRING((LPSTR)s.c_str());
  // 0x9040DFB09BE75706 = _END_TEXT_COMMAND_GET_NUMBER_OF_LINES(x, y).
  int n = invoke<int>(0x9040DFB09BE75706, wrapStartFrac, 0.0f);
  UI::SET_TEXT_WRAP(0.0f, 1.0f);
  return n < 1 ? 1 : n;
}

void Sprite(const char *dict, const char *name, float xc, float yc, float w,
            float h, const Color &tint) {
  if (!GRAPHICS::HAS_STREAMED_TEXTURE_DICT_LOADED((char *)dict)) return;
  GRAPHICS::DRAW_SPRITE((char *)dict, (char *)name, xc, yc, w, h, 0.0f, tint.r,
                        tint.g, tint.b, tint.a);
}

bool EnsureTxd(const char *dict) {
  if (GRAPHICS::HAS_STREAMED_TEXTURE_DICT_LOADED((char *)dict)) return true;
  GRAPHICS::REQUEST_STREAMED_TEXTURE_DICT((char *)dict, FALSE);
  return GRAPHICS::HAS_STREAMED_TEXTURE_DICT_LOADED((char *)dict);
}

} // namespace draw
} // namespace gtam
