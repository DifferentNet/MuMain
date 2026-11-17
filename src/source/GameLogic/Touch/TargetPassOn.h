// The foe the player chose passing on as it dies (pad round 5, 2026-11-17).
//
// The right stick or the touch Target button chooses a foe, and the buttons fight it. As it died the
// choice was dropped and the buttons fell back to the nearest foe, often one behind the hero, so the
// player chose again after every kill. Now a choice still the player's (young, or still fought) passes
// on to the next foe the way the dead one lay from the hero, the nearest there first, as the right
// stick would pick it. A choice left alone for a while is dropped as before.
#pragma once

#include "GameLogic/Touch/DirectionPick.h"

#include <chrono>
#include <vector>

namespace GameLogic::Touch::TargetPassOn
{
using Clock = std::chrono::steady_clock;

// A choice made this recently passes on: one older and no longer fought is not the player's any more.
inline constexpr auto ChoiceAge = std::chrono::seconds(10);
// Or one the buttons fought this recently: a long fight keeps the player's way.
inline constexpr auto FightAge = std::chrono::seconds(3);

// Whether the choice passes on as its foe dies: made within ChoiceAge, fought within FightAge, or
// the hero fighting still (the engine's own attack under way).
[[nodiscard]] bool Passes(Clock::time_point chosenAt, Clock::time_point foughtAt, bool fighting, Clock::time_point now);

// The foe it passes on to, never `gone`: the way (goneX, goneY) lay from the hero (heroX, heroY), the
// nearest within the cone first, else the one at the smallest angle (DirectionPick::Pick); NoKey with
// none. All in the map's tiles.
[[nodiscard]] int Next(const std::vector<DirectionPick::Candidate>& candidates, float heroX, float heroY, float goneX,
                       float goneY, int gone);
} // namespace GameLogic::Touch::TargetPassOn
