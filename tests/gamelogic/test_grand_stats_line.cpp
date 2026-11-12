#include "doctest.h"

#include <string>

#include "GameLogic/Items/GrandStatsLine.h"

using GameLogic::Items::GrandStatsLine::Parse;
using GameLogic::Items::GrandStatsLine::Points;

TEST_CASE("The Grand stats line gives the five stats in order")
{
    const auto points = Parse(L"#GRS|3000|2000|10|4|5");
    REQUIRE(points.has_value());
    CHECK(points->strength == 3000);
    CHECK(points->agility == 2000);
    CHECK(points->vitality == 10);
    CHECK(points->energy == 4);
    CHECK(points->command == 5);

    CHECK(*Parse(L"#GRS|0|0|0|0|0") == Points{});
    CHECK(Parse(L"#GRS|1|2|3|4|5\r\n")->command == 5);
    CHECK(Parse(L"#GRS|1|2|3|4|5 ")->command == 5);
    CHECK(Parse(L"#GRS|2147483647|0|0|0|0")->strength == 2147483647);
}

TEST_CASE("The Grand stats line refuses bad lines")
{
    CHECK_FALSE(Parse(L"").has_value());
    CHECK_FALSE(Parse(L"#GRS|").has_value());
    CHECK_FALSE(Parse(L"#GRS|1|2|3|4").has_value());
    CHECK_FALSE(Parse(L"#GRS|1|2|3|4|5|6").has_value());
    CHECK_FALSE(Parse(L"#GRS|1|2|3|4|").has_value());
    CHECK_FALSE(Parse(L"#GRS||2|3|4|5").has_value());
    CHECK_FALSE(Parse(L"#GRS|-1|2|3|4|5").has_value());
    CHECK_FALSE(Parse(L"#GRS|+1|2|3|4|5").has_value());
    CHECK_FALSE(Parse(L"#GRS|1|x|3|4|5").has_value());
    CHECK_FALSE(Parse(L"#GRS| 1|2|3|4|5").has_value());
    CHECK_FALSE(Parse(L"#GRS|2147483648|0|0|0|0").has_value());
    CHECK_FALSE(Parse(L"#GRS|99999999999|0|0|0|0").has_value());
    CHECK_FALSE(Parse(L"#ANC|1|2|3|4|5").has_value());
    CHECK_FALSE(Parse(L"GRS|1|2|3|4|5").has_value());
}
