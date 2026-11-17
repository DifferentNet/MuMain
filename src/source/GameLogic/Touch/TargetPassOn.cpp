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
    // One dead on the hero's own tile lay no way at all: the plain nearest then (Pick takes every
    // candidate as ahead).
    return DirectionPick::Pick(candidates, heroX, heroY, goneX - heroX, goneY - heroY, gone, true);
}
} // namespace GameLogic::Touch::TargetPassOn
