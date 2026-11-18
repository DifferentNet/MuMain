#include <doctest.h>

#include "GameLogic/Items/JewelUpgradeResult.h"

using namespace GameLogic::Items::JewelUpgrade;

TEST_CASE("jewel result: what changed on the item [items]")
{
    CHECK(Compare({6, 0}, {7, 0}) == Outcome::LevelUp);
    CHECK(Compare({7, 1}, {6, 1}) == Outcome::LevelDown);
    CHECK(Compare({7, 1}, {0, 1}) == Outcome::LevelDown);
    CHECK(Compare({9, 1}, {9, 2}) == Outcome::OptionUp);
    CHECK(Compare({9, 2}, {9, 2}) == Outcome::None);
    // Life failing lowers or takes away the option.
    CHECK(Compare({9, 2}, {9, 0}) == Outcome::OptionDown);
    CHECK(Compare({9, 3}, {9, 2}) == Outcome::OptionDown);
    // A level change says more than the option.
    CHECK(Compare({9, 2}, {8, 0}) == Outcome::LevelDown);
}

TEST_CASE("jewel result: only the slot the jewel was used on, once, in time [items]")
{
    RememberTarget(12, 1'000);
    CHECK_FALSE(TakeTarget(13, 1'100));
    CHECK(TakeTarget(12, 1'200));
    CHECK_FALSE(TakeTarget(12, 1'300));

    RememberTarget(20, 5'000);
    CHECK_FALSE(TakeTarget(20, 5'000 + AnswerWindowMs + 1));
    CHECK_FALSE(TakeTarget(20, 5'000 + AnswerWindowMs + 2));
}
