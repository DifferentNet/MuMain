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
inline constexpr std::array<int, 5> Percents = {80, 90, 100, 115, 130};

// The choice nearest a saved percent (a hand edit of config.ini may hold any number).
[[nodiscard]] int NearestIndex(int percent);

// One finger unit in CSS pixels, for the game's area on the page (width x height in CSS pixels) and
// the player's percent. Never so big that the controls' column (the top row, the hero frame, the
// potions over the joystick) no longer fits the area's height.
[[nodiscard]] float UnitCssPixels(float areaWidth, float areaHeight, int percent);
} // namespace UI::Mobile::ButtonSize
