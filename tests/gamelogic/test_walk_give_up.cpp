// Tests of when the MU Helper stops trying to walk back to its start spot for a while and fights instead
// (GameLogic::Automation::ShouldGiveUpWalk). Ringed by monsters it found no way back, and the round ended on the
// walk every time: it stood among them without attacking (players and ghinzuro, 2026-11-03).

#include <doctest.h>

#include "GameLogic/Automation/Movement.h"

using GameLogic::Automation::MoveResult;
using GameLogic::Automation::ShouldGiveUpWalk;

namespace
{
constexpr long long Patience = 2000;
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
