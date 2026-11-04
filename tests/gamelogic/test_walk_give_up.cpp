// Tests of when the MU Helper stops trying to walk back to its start spot for a while and fights instead
// (GameLogic::Automation::ShouldGiveUpWalk). Ringed by monsters it found no way back, and the round ended on the
// walk every time: it stood among them without attacking (players and ghinzuro, 2026-11-03).

#include <doctest.h>

#include "GameLogic/Automation/Movement.h"

using GameLogic::Automation::MoveResult;
using GameLogic::Automation::IsWalkProgress;
using GameLogic::Automation::ShouldGiveUpWalk;
using GameLogic::Automation::WalkGap;

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

TEST_CASE("A shorter path to the spot is progress, the same or a longer one is not [gamelogic][walk]")
{
    CHECK(IsWalkProgress(WalkGap(true, 7, 5), WalkGap(true, 8, 5)));
    // Stepping back and forth among monsters near the spot: the path does not get shorter.
    CHECK_FALSE(IsWalkProgress(WalkGap(true, 8, 5), WalkGap(true, 8, 5)));
    CHECK_FALSE(IsWalkProgress(WalkGap(true, 9, 5), WalkGap(true, 8, 5)));
}

TEST_CASE("A detour round a wall to the spot is progress while its path gets shorter [gamelogic][walk]")
{
    // The straight line grows going round, the path to the spot still shrinks.
    CHECK(IsWalkProgress(WalkGap(true, 10, 6), WalkGap(true, 11, 4)));
}

TEST_CASE("A short way to a free tile among monsters is not nearly there [gamelogic][walk]")
{
    // Kanturu 2, 2026-11-04: ringed by monsters 6 tiles from the spot, the path finder gives 2 or 3 steps to the
    // nearest free tile. That used to count as getting closer every round, so the walk never got given up.
    const int iBest = WalkGap(false, 3, 6);
    CHECK_FALSE(IsWalkProgress(WalkGap(false, 2, 6), iBest));
    CHECK_FALSE(IsWalkProgress(WalkGap(false, 3, 7), iBest));
    // A step nearer the spot in a straight line is progress, whatever the partial path's length.
    CHECK(IsWalkProgress(WalkGap(false, 4, 5), iBest));
}

TEST_CASE("Any path that reaches the spot beats one that does not [gamelogic][walk]")
{
    CHECK(IsWalkProgress(WalkGap(true, 14, 9), WalkGap(false, 2, 3)));
    CHECK_FALSE(IsWalkProgress(WalkGap(false, 2, 3), WalkGap(true, 14, 9)));
}

TEST_CASE("Further than the path finder looks, only getting nearer is progress [gamelogic][walk]")
{
    // 20 tiles away the path is cut at 15 steps every round and ends short of the spot.
    CHECK(IsWalkProgress(WalkGap(false, MaxSteps, 19), WalkGap(false, MaxSteps, 20)));
    CHECK_FALSE(IsWalkProgress(WalkGap(false, MaxSteps, 20), WalkGap(false, MaxSteps, 20)));
}
