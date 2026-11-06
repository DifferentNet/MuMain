// Tests of when the MU Helper sets a drop aside for a while and fights instead
// (GameLogic::Automation::ShouldSetAsidePickup). A drop under the monsters or behind a ring of them had no way to it,
// and the round ended on the pickup every time: the helper stood in big packs without attacking (ghinzuro's Dark
// Knight, 2026-11-06).

#include <doctest.h>

#include "GameLogic/Automation/Pickup.h"

using GameLogic::Automation::PickupResult;
using GameLogic::Automation::ShouldPutOffPickup;
using GameLogic::Automation::ShouldSetAsidePickup;
using GameLogic::Automation::ShouldUnstickFight;

namespace
{
constexpr long long Patience = 2000;
} // namespace

TEST_CASE("A drop with no way to it is set aside at once [gamelogic][pickup]")
{
    CHECK(ShouldSetAsidePickup(PickupResult::NoPath, 0, Patience));
    CHECK(ShouldSetAsidePickup(PickupResult::NoPath, 5000, Patience));
}

TEST_CASE("A walk to a drop that gets closer goes on [gamelogic][pickup]")
{
    CHECK_FALSE(ShouldSetAsidePickup(PickupResult::Approaching, 0, Patience));
    CHECK_FALSE(ShouldSetAsidePickup(PickupResult::Approaching, Patience - 1, Patience));
}

TEST_CASE("A walk to a drop that gets no closer for the patience is set aside [gamelogic][pickup]")
{
    CHECK(ShouldSetAsidePickup(PickupResult::Approaching, Patience, Patience));
    CHECK(ShouldSetAsidePickup(PickupResult::Approaching, Patience * 10, Patience));
}

TEST_CASE("A pickup sent, waiting, gone or too far is never set aside [gamelogic][pickup]")
{
    for (const PickupResult result : {PickupResult::Requested, PickupResult::Busy, PickupResult::Gone, PickupResult::TooFar})
    {
        CHECK_FALSE(ShouldSetAsidePickup(result, 0, Patience));
        CHECK_FALSE(ShouldSetAsidePickup(result, Patience * 10, Patience));
    }
}

TEST_CASE("While a monster is within reach a drop that takes a walk waits [gamelogic][pickup]")
{
    constexpr long long FightWait = 10000;
    CHECK(ShouldPutOffPickup(true, true, 0, FightWait));
    CHECK(ShouldPutOffPickup(true, true, FightWait - 1, FightWait));
}

TEST_CASE("A drop next to the character, or with no monster in reach, is taken at once [gamelogic][pickup]")
{
    constexpr long long FightWait = 10000;
    CHECK_FALSE(ShouldPutOffPickup(false, true, 0, FightWait));
    CHECK_FALSE(ShouldPutOffPickup(true, false, 0, FightWait));
    CHECK_FALSE(ShouldPutOffPickup(false, false, 0, FightWait));
}

TEST_CASE("A drop put off for the whole fight wait gets a walk anyway [gamelogic][pickup]")
{
    constexpr long long FightWait = 10000;
    CHECK_FALSE(ShouldPutOffPickup(true, true, FightWait, FightWait));
    CHECK_FALSE(ShouldPutOffPickup(true, true, FightWait * 5, FightWait));
}

TEST_CASE("Standing among monsters without a hit for the stall limit starts the fight over [gamelogic][pickup]")
{
    constexpr long long StallLimit = 4000;
    CHECK(ShouldUnstickFight(true, true, StallLimit, StallLimit));
    CHECK(ShouldUnstickFight(true, true, StallLimit * 3, StallLimit));
    CHECK_FALSE(ShouldUnstickFight(true, true, StallLimit - 1, StallLimit));
}

TEST_CASE("A walking character, or one with no monster close by, is left alone [gamelogic][pickup]")
{
    constexpr long long StallLimit = 4000;
    CHECK_FALSE(ShouldUnstickFight(false, true, StallLimit * 3, StallLimit));
    CHECK_FALSE(ShouldUnstickFight(true, false, StallLimit * 3, StallLimit));
}
