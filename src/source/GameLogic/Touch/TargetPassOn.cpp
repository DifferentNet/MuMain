#include "GameLogic/Touch/TargetPassOn.h"

namespace GameLogic::Touch::TargetPassOn
{
bool Passes(Clock::time_point chosenAt, Clock::time_point foughtAt, bool fighting, Clock::time_point now)
{
    return fighting || now - chosenAt < ChoiceAge || now - foughtAt < FightAge;
}

int Next(const std::vector<DirectionPick::Candidate>& candidates, float heroX, float heroY, float goneX, float goneY,
         int gone)
{
    // Only that way: a foe behind the hero is not where the fight was going, the choice is dropped.
    // One dead on the hero's own tile lay no way at all: the plain nearest then (Pick takes every
    // candidate as ahead).
    return DirectionPick::Pick(candidates, heroX, heroY, goneX - heroX, goneY - heroY, gone, false);

}
} // namespace GameLogic::Touch::TargetPassOn
