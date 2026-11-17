// One-time hints for a player new to the pad (pad round 5, 2026-11-17).
//
// The help's pad page and the controls card say everything, but a player meets each thing first in play:
// an item dropped by the hero, a window opened, the bag full, a death. Each such moment says in the system
// messages, once ever, which button does it there ({A}, {B} in the names of the pad in hand). Only while
// the pad is in use: a mouse player never sees them, and they wait for the pad's first such moment.
#pragma once

namespace UI::HUD::PadHints
{
// The moments, one bit each in [UI] GamepadHintsSeen: keep their values, the config file remembers them.
enum class Hint
{
    PickUp = 0,
    Window = 1,
    BagFull = 2,
    Death = 3,
};

// Whether `bits` (GamepadHintsSeen) says the hint showed already, and `bits` with it marked shown.
[[nodiscard]] bool Seen(int bits, Hint hint);
[[nodiscard]] int Marked(int bits, Hint hint);

// Says the hint in the system messages, if the pad is in use and it never showed before.
void Show(Hint hint);
} // namespace UI::HUD::PadHints
