// How a gamepad stick's place reads: the rest around its centre, and when a lean counts as a push.
// Worn sticks drift, resting a little off their centre and wandering there; none of this may walk the
// hero or take the game from the mouse.
#pragma once

#include "Core/Input/GamepadInput.h"

namespace Core::Input::Gamepad
{
// The share of a stick's throw around its centre that reads as rest: about XInput's advice for an
// Xbox pad (7849 and 8689 of 32767 for the left and the right stick).
inline constexpr float StickDeadZone = 0.25f;

// A stick's raw place (-1 .. 1 each axis) past the dead zone, the same way: (0, 0) inside it, then a
// length growing from 0 at its edge to 1 at the rim. Round, so a diagonal is not cut short.
[[nodiscard]] StickPosition PastDeadZone(float x, float y);

// How far a stick leans past its dead zone (0 .. 1).
[[nodiscard]] float Lean(StickPosition stick);

// Whether a stick past its dead zone counts as pushed, given whether it did the frame before: a push
// starts a little further out than it ends, so a stick resting near the edge neither starts one nor
// starts and stops in turn.
[[nodiscard]] bool IsPushed(StickPosition stick, bool wasPushed);
} // namespace Core::Input::Gamepad
