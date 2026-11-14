// The on-screen controls' size (touch_controls.rml): measured in finger units, which follow the
// screen's real size rather than the game's resolution, so a button is as wide as a thumb on a
// small phone and no wider than one on a tablet. One unit is one CSS pixel on a phone, a little more
// on a bigger screen, times the player's choice in Options -> Interface (Button size).
//
// The sums only; UI::Mobile::TouchUnit applies them to the game's window.
#pragma once

#include <array>

namespace UI::Mobile::ButtonSize
{
// The choices Options offers, in percent, ascending; 100 is the default.
inline constexpr std::array<int, 7> Percents = {60, 70, 80, 90, 100, 115, 130};

// The choice nearest a saved percent (a hand edit of config.ini may hold any number).
[[nodiscard]] int NearestIndex(int percent);

// The phone HUD's top row at its smallest, in units (touch_controls.rcss, mobile-hud: the row's font is
// clamp(9rem, (100vw - 214dp - fixed) / per-unit, 12rem)): the fixed parts with the right margin, and the
// long buttons at the 9-unit font. Fingers: 332 + 21.4 x 9; with the pad's glyphs (body.pad): 278 + 30.9 x 9.
inline constexpr float TouchTopRowUnits = 332.f + 21.4f * 9.f;
inline constexpr float PadTopRowUnits = 278.f + 30.9f * 9.f;

// One finger unit in CSS pixels, for the game's area on the page (width x height in CSS pixels) and
// the player's percent. Never so big that the controls' column (the top row, the hero frame, the
// potions over the joystick) no longer fits the area's height. topRowUnits (the phone's HUD only, one
// of the two above; 0 elsewhere) also keeps it so small that the top row at its smallest fits the
// area's width beside the mini map; dpCssPixels is one RmlUi dp in CSS pixels (the map's corner is in dp).
[[nodiscard]] float UnitCssPixels(float areaWidth, float areaHeight, int percent, float topRowUnits = 0.f,
                                  float dpCssPixels = 1.f);
} // namespace UI::Mobile::ButtonSize
