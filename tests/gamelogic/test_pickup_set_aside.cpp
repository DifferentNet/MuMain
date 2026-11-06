// Tests of when the MU Helper sets a drop aside for a while and fights instead
// (GameLogic::Automation::ShouldSetAsidePickup). A drop under the monsters or behind a ring of them had no way to it,
// and the round ended on the pickup every time: the helper stood in big packs without attacking (ghinzuro's Dark
// Knight, 2026-11-06).

#include <doctest.h>

#include "GameLogic/Automation/Pickup.h"

using GameLogic::Automation::PickupResult;
using GameLogic::Automation::ShouldSetAsidePickup;

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
