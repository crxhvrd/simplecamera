/*
        MenuDraw — shared low-level drawing primitives for the NativeMenu /
        CommandPalette GUI. Thin wrappers over ScriptHookV GRAPHICS/UI natives,
        authored on a fixed 1920x1080 design grid so everything is
        resolution-independent. Both the panel menu and the command palette draw
        through these so they stay visually consistent.

        This header is native-free (declarations only); MenuDraw.cpp is the only
        place that pulls in the ScriptHookV headers.
*/

#pragma once

#include "NativeMenu.h" // for gtam::Color

#include <string>
#include <vector>

namespace gtam {
namespace draw {

// Design grid. All px arguments below are in these units; they are converted to
// screen fractions internally, so 1 unit looks the same at 1080p / 1440p / 4K.
constexpr float BASE_W = 1920.0f;
constexpr float BASE_H = 1080.0f;

inline float PxX(float px) { return px / BASE_W; }
inline float PxY(float px) { return px / BASE_H; }

enum class Align { Left, Center, Right };

// Filled rectangle, top-left anchored, coordinates as screen fractions (0..1).
void RectTL(float xFrac, float yFrac, float wFrac, float hFrac, const Color &c);

// 1px (design) outline rectangle.
void RectOutline(float xFrac, float yFrac, float wFrac, float hFrac,
                 const Color &c, float thicknessPx = 1.0f);

// Draw a string. `scale` is the GTA text scale; `font` a GTA font id. For
// Align::Right pass the right edge fraction the text should end at.
void Text(const std::string &s, float xFrac, float yFrac, float scale, int font,
          const Color &c, Align align, float rightEdgeFrac = 0.0f);

// Width of `s` as a screen fraction, for the given scale/font. Lets callers do
// segmented/centered layout and fuzzy-match highlight runs.
float TextWidth(const std::string &s, float scale, int font);

// Height of a single text line as a screen fraction, for the given scale/font.
// Use to vertically centre text in a row: y = top + (rowH - LineHeight)/2.
float TextLineHeight(float scale, int font);

// Number of lines `s` wraps to within [wrapStartFrac, wrapEndFrac] at the given
// scale/font. Lets a panel auto-size to its (possibly multi-line) text.
int TextLineCount(const std::string &s, float scale, int font,
                  float wrapStartFrac, float wrapEndFrac);

// Word-wrap `s` into lines that each fit within `maxWidthFrac` at the given
// scale/font, measured with our own TextWidth. Use this instead of the engine's
// SET_TEXT_WRAP when you need both an exact line count AND strings short enough
// to dodge GTA's 99-char-per-DRAW truncation: draw each returned line on its own
// row. A single word longer than the width is kept whole on its own line.
std::vector<std::string> TextWrap(const std::string &s, float scale, int font,
                                  float maxWidthFrac);

// Draw a sprite (texture) from a streamed dictionary, centre-anchored, with a
// tint. Width/height are screen fractions. No-op until the dict is loaded.
void Sprite(const char *dict, const char *name, float xCenterFrac,
            float yCenterFrac, float wFrac, float hFrac, const Color &tint);

// Request a streamed texture dictionary (call each frame you intend to draw
// from it). Returns true once it is resident and safe to DRAW_SPRITE from.
bool EnsureTxd(const char *dict);

} // namespace draw
} // namespace gtam
