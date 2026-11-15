#include "doctest.h"

#include <ostream>   // doctest's string_view stringifier needs it complete
#include <string_view>

#include "GameLogic/Items/DurabilityBand.h"

using GameLogic::Items::ClassifyDurability;
using GameLogic::Items::DurabilityBand;
using GameLogic::Items::DurabilityBandName;

TEST_CASE("Durability bands follow the original's 50 / 30 / 20 % steps")
{
    CHECK(ClassifyDurability(100, 100) == DurabilityBand::Fine);
    CHECK(ClassifyDurability(51, 100) == DurabilityBand::Fine);
    CHECK(ClassifyDurability(50, 100) == DurabilityBand::Half);
    CHECK(ClassifyDurability(31, 100) == DurabilityBand::Half);
    CHECK(ClassifyDurability(30, 100) == DurabilityBand::Third);
    CHECK(ClassifyDurability(21, 100) == DurabilityBand::Third);
    CHECK(ClassifyDurability(20, 100) == DurabilityBand::Fifth);
    CHECK(ClassifyDurability(1, 100) == DurabilityBand::Fifth);
    CHECK(ClassifyDurability(0, 100) == DurabilityBand::Zero);
}

TEST_CASE("Odd maximums compare against the exact fraction")
{
    // 255 * 0.5 = 127.5: 127 is at most half, 128 is not.
    CHECK(ClassifyDurability(128, 255) == DurabilityBand::Fine);
    CHECK(ClassifyDurability(127, 255) == DurabilityBand::Half);
    // 255 * 0.2 = 51.
    CHECK(ClassifyDurability(51, 255) == DurabilityBand::Fifth);
    CHECK(ClassifyDurability(52, 255) == DurabilityBand::Third);
}

TEST_CASE("Band names match item_endurance.rcss")
{
    CHECK(std::string_view(DurabilityBandName(DurabilityBand::Fine)).empty());
    CHECK(std::string_view(DurabilityBandName(DurabilityBand::Half)) == "half");
    CHECK(std::string_view(DurabilityBandName(DurabilityBand::Third)) == "third");
    CHECK(std::string_view(DurabilityBandName(DurabilityBand::Fifth)) == "fifth");
    CHECK(std::string_view(DurabilityBandName(DurabilityBand::Zero)) == "zero");
}
