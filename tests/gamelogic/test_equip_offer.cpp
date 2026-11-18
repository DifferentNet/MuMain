#include <doctest.h>

#include "GameLogic/Items/EquipOffer.h"
#include "GameLogic/Items/EquipOfferTracker.h"

namespace EquipOffer = GameLogic::Items::EquipOffer;
using EquipOffer::Candidate;
using EquipOffer::EquipOfferTracker;

namespace
{
Candidate Better(int bagIndex, int gain, std::uint32_t key = 1)
{
    Candidate candidate;
    candidate.bagIndex = bagIndex;
    candidate.itemKey = key;
    candidate.combatPowerChange = gain;
    candidate.canWear = true;
    candidate.canPutOnNow = true;
    return candidate;
}
} // namespace

TEST_CASE("only wearable items that add Combat Power and fit with one move are offered [gamelogic][equip-offer]")
{
    CHECK(EquipOffer::ShouldOffer(Better(20, 5)));
    CHECK_FALSE(EquipOffer::ShouldOffer(Better(20, 0)));
    CHECK_FALSE(EquipOffer::ShouldOffer(Better(20, -10)));
    CHECK_FALSE(EquipOffer::ShouldOffer(Better(-1, 5)));
    Candidate tooWeak = Better(20, 5);
    tooWeak.canWear = false;
    CHECK_FALSE(EquipOffer::ShouldOffer(tooWeak));
    Candidate twoOff = Better(20, 5);
    twoOff.canPutOnNow = false;
    CHECK_FALSE(EquipOffer::ShouldOffer(twoOff));
}

TEST_CASE("an offer runs out after its lifetime [gamelogic][equip-offer]")
{
    EquipOfferTracker tracker;
    CHECK(tracker.Current(0) == nullptr);
    CHECK(tracker.Consider(Better(20, 5), 1000));
    REQUIRE(tracker.Current(1000) != nullptr);
    CHECK(tracker.Current(1000)->bagIndex == 20);
    CHECK(tracker.Current(1000 + EquipOffer::LifetimeMs - 1) != nullptr);
    CHECK(tracker.Current(1000 + EquipOffer::LifetimeMs) == nullptr);
}

TEST_CASE("a better pick-up replaces the offer, a weaker one does not [gamelogic][equip-offer]")
{
    EquipOfferTracker tracker;
    tracker.Consider(Better(20, 50), 0);
    CHECK_FALSE(tracker.Consider(Better(21, 10), 100));
    CHECK(tracker.Current(100)->bagIndex == 20);
    CHECK(tracker.Consider(Better(22, 80), 200));
    CHECK(tracker.Current(200)->bagIndex == 22);
    // The new offer's time starts again.
    CHECK(tracker.Current(200 + EquipOffer::LifetimeMs - 1) != nullptr);
    // Once the offer ran out any good pick-up shows.
    CHECK(tracker.Consider(Better(23, 1), 200 + EquipOffer::LifetimeMs));
}

TEST_CASE("the offer goes when its item leaves the bag or it is cleared [gamelogic][equip-offer]")
{
    EquipOfferTracker tracker;
    tracker.Consider(Better(20, 5), 0);
    tracker.ItemLeft(21);
    CHECK(tracker.Current(1) != nullptr);
    tracker.ItemLeft(20);
    CHECK(tracker.Current(1) == nullptr);
    tracker.Consider(Better(20, 5), 2);
    tracker.Clear();
    CHECK(tracker.Current(3) == nullptr);
    // A pick-up that should not be offered never shows.
    CHECK_FALSE(tracker.Consider(Better(20, 0), 4));
    CHECK(tracker.Current(4) == nullptr);
}

TEST_CASE("a tap right after the offer appears is not taken [gamelogic][equip-offer]")
{
    EquipOfferTracker tracker;
    REQUIRE(tracker.Consider(Better(20, 5), 1000));
    const EquipOffer::Offer* offer = tracker.Current(1100);
    REQUIRE(offer != nullptr);
    CHECK_FALSE(EquipOffer::TakesTap(*offer, 1100));
    CHECK_FALSE(EquipOffer::TakesTap(*offer, 1000 + EquipOffer::TapGuardMs - 1));
    CHECK(EquipOffer::TakesTap(*offer, 1000 + EquipOffer::TapGuardMs));
    // A better pick-up starts the guard again.
    REQUIRE(tracker.Consider(Better(30, 9), 2000));
    CHECK_FALSE(EquipOffer::TakesTap(*tracker.Current(2100), 2100));
}
