// One-time hints for a player new to the pad (pad round 5, 2026-11-17).
//
// The help's pad page and the controls card say everything, but a player meets each thing first in play:
// an item dropped by the hero, a window opened, the bag full, a death. Each such moment says in the system
// messages, once ever, which button does it there ({A}, {B} in the names of the pad in hand). Only while
// the pad is in use: a mouse player never sees them, and they wait for the pad's first such moment.
#pragma once

// The moments (Hint), their bits and when the small screen hint applies.
#include "UI/HUD/PadHintBits.h"

namespace UI::HUD::PadHints
{
// Says the hint in the system messages, if the pad is in use and it never showed before.
void Show(Hint hint);
} // namespace UI::HUD::PadHints
