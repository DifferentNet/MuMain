#pragma once

#include <cstdint>

// Web build only. A browser shows a gamepad to the page only once one of its buttons is pressed, and
// SDL takes the pad's state at that moment for its starting state: the press that woke the pad
// reaches the game as neither a press nor a release (Core/Input/GamepadInput reads it here instead).
namespace mu::web
{
// The buttons the browser's "standard" gamepads hold now, bit n for the Gamepad API's button n: A, B,
// X, Y, LB, RB, LT, RT, View, Menu, LS, RS, then the d-pad's up, down, left and right. A trigger
// counts as held once pulled past `triggerPull` (0 .. 1).
[[nodiscard]] std::uint32_t HeldStandardButtons(float triggerPull);
} // namespace mu::web
