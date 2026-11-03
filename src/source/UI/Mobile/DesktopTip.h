// MU Moldova: the help tips that scroll across the top (Slide_<lang>.bmd) were written for a mouse and
// a keyboard ("right click on monster", "press Q, W, E", "ctrl+number"). With the on-screen controls
// (fingers or a gamepad) those tips point at buttons the player hasn't got, so they are left out there.
#pragma once

#include <string_view>

namespace UI::Mobile::DesktopTip
{
// Whether the tip tells the player to use the mouse or a key: it names a click, a key press, the
// cursor or a modifier key (any case).
[[nodiscard]] bool NeedsMouseOrKeys(std::wstring_view tip);
} // namespace UI::Mobile::DesktopTip
