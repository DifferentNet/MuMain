// Tests of when the MU Helper stops trying to walk back to its start spot for a while and fights instead
// (GameLogic::Automation::ShouldGiveUpWalk). Ringed by monsters it found no way back, and the round ended on the
// walk every time: it stood among them without attacking (players and ghinzuro, 2026-11-03).

#include <doctest.h>

#include "GameLogic/Automation/Movement.h"

using GameLogic::Automation::MoveResult;
using GameLogic::Automation::IsWalkProgress;
using GameLogic::Automation::ShouldGiveUpWalk;

namespace
{
constexpr long long Patience = 2000;
constexpr int MaxSteps = 15;
} // namespace

TEST_CASE("No way back is given up at once [gamelogic][walk]")
{
    CHECK(ShouldGiveUpWalk(MoveResult::NoPath, 0, Patience));
    CHECK(ShouldGiveUpWalk(MoveResult::NoPath, 5000, Patience));
}

TEST_CASE("A walk that gets closer goes on [gamelogic][walk]")
{
    CHECK_FALSE(ShouldGiveUpWalk(MoveResult::Walking, 0, Patience));
    CHECK_FALSE(ShouldGiveUpWalk(MoveResult::Walking, Patience - 1, Patience));
}

TEST_CASE("A walk that gets no closer for the patience is given up [gamelogic][walk]")
{
    CHECK(ShouldGiveUpWalk(MoveResult::Walking, Patience, Patience));
    CHECK(ShouldGiveUpWalk(MoveResult::Walking, Patience * 10, Patience));
}

TEST_CASE("Arriving is never giving up [gamelogic][walk]")
{
    CHECK_FALSE(ShouldGiveUpWalk(MoveResult::Arrived, 0, Patience));
    CHECK_FALSE(ShouldGiveUpWalk(MoveResult::Arrived, Patience * 10, Patience));
}

TEST_CASE("A shorter path is progress, the same or a longer one is not [gamelogic][walk]")
{
    CHECK(IsWalkProgress(7, 8, MaxSteps, true));
    CHECK(IsWalkProgress(7, 8, MaxSteps, false));
    // Stepping back and forth among monsters near the spot: the tile changes, the path does not get shorter.
    CHECK_FALSE(IsWalkProgress(8, 8, MaxSteps, true));
    CHECK_FALSE(IsWalkProgress(9, 8, MaxSteps, true));
}

TEST_CASE("Further than the path finder looks, moving on is progress [gamelogic][walk]")
{
    // 20 tiles away the path is 15 steps every round: walking on to another tile counts.
    CHECK(IsWalkProgress(MaxSteps, MaxSteps, MaxSteps, true));
    CHECK_FALSE(IsWalkProgress(MaxSteps, MaxSteps, MaxSteps, false));
}
