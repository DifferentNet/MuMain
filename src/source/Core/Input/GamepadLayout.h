// The player's button layout for the pad (Options -> Interface -> Gamepad layout, [UI] GamepadLayout):
// which physical button or stick plays which of the game's own. The game reads the pad by place
// (GamepadInput.h); a preset only trades places between pairs, so each preset is its own inverse:
// the button a physical one plays is also the physical one that plays it.
//
// A, B, X, Y, View, Menu and the d-pad never move: the windows' buttons stay where they are on every
// preset (pad round 6, B1).
#pragma once

#include "Core/Input/GamepadInput.h"

#include <cstdint>

namespace Core::Input::Gamepad
{
enum class Layout : std::uint8_t
{
    // As the pad always played.
    Standard,
    // The sticks trade places, and their clicks with them: the right stick walks, the left one picks
    // targets and plays the pointer.
    LeftHanded,
    // LB with LT and RB with RT: the potions go to the triggers, the second skill page to RB.
    SwappedShoulders,
    Count,
};
inline constexpr int LayoutCount = static_cast<int>(Layout::Count);

// The layout of a saved number; an unknown one (a hand-edited config.ini) is Standard.
[[nodiscard]] Layout LayoutFromIndex(int index);

// The button the other side of the layout's pair: the game's button a physical one plays, or the
// physical button that plays a game's one (the same, as each preset is its own inverse).
[[nodiscard]] Button Swapped(Button button, Layout layout);

// Whether the left and right sticks trade places.
[[nodiscard]] bool SwapsSticks(Layout layout);
// Whether the bumpers and the triggers trade places.
[[nodiscard]] bool SwapsShoulders(Layout layout);
} // namespace Core::Input::Gamepad
