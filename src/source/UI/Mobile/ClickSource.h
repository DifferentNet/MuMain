// Which device a click on an item came from. A mouse's click acts at once, as it always has; a
// finger's tap, a finger's long press and the pad's right stick button first show what the click
// will do (UI::Mobile::ItemCard) or wait for a second tap, as on a phone, where a finger brushing
// past must not sell, buy or take off anything.
//
// Ask on the frame a window sees the press or the release (mu::ui::window::IsPress, IsRelease).
#pragma once

namespace UI::Mobile::ClickSource
{
// The left button came from a finger's tap: pressed and let go where the finger lifted.
[[nodiscard]] bool IsFingerTap();
// The left button came from a finger that moved off where it landed: a drag, or a swipe.
[[nodiscard]] bool IsFingerDrag();
// The right button comes from a finger held still (the long press).
[[nodiscard]] bool IsFingerLongPress();
// The right button comes from the pad's right stick button while the pad plays the game.
[[nodiscard]] bool IsPadRightClick();
} // namespace UI::Mobile::ClickSource
