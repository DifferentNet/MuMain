#include "doctest.h"

#include <string>

#include "GameLogic/Character/GrandResetCountLine.h"

using GameLogic::GrandResetCountLine::Counts;
using GameLogic::GrandResetCountLine::Parse;

TEST_CASE("The Grand Reset count line gives the Grand Resets and the resets")
{
    const auto counts = Parse(L"#GRC|1|0");
    REQUIRE(counts.has_value());
    CHECK(counts->grandResets == 1);
    CHECK(counts->resets == 0);

    CHECK(*Parse(L"#GRC|0|87") == Counts{0, 87});
    CHECK(*Parse(L"#GRC|2|40") == Counts{2, 40});
    CHECK(*Parse(L"#GRC|0|0") == Counts{});
    CHECK(Parse(L"#GRC|5|100\r\n")->resets == 100);
    CHECK(Parse(L"#GRC|5|100 ")->grandResets == 5);
    CHECK(Parse(L"#GRC|0|2147483647")->resets == 2147483647);
}

TEST_CASE("The Grand Reset count line refuses bad lines")
{
    CHECK_FALSE(Parse(L"").has_value());
    CHECK_FALSE(Parse(L"#GRC|").has_value());
    CHECK_FALSE(Parse(L"#GRC|1").has_value());
    CHECK_FALSE(Parse(L"#GRC|1|").has_value());
    CHECK_FALSE(Parse(L"#GRC||0").has_value());
    CHECK_FALSE(Parse(L"#GRC|1|2|3").has_value());
    CHECK_FALSE(Parse(L"#GRC|-1|0").has_value());
    CHECK_FALSE(Parse(L"#GRC|+1|0").has_value());
    CHECK_FALSE(Parse(L"#GRC|1|x").has_value());
    CHECK_FALSE(Parse(L"#GRC| 1|0").has_value());
    CHECK_FALSE(Parse(L"#GRC|1|1,000").has_value());
    CHECK_FALSE(Parse(L"#GRC|2147483648|0").has_value());
    CHECK_FALSE(Parse(L"#GRS|1|0").has_value());
    CHECK_FALSE(Parse(L"GRC|1|0").has_value());
}
