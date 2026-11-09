#include "doctest.h"

#include <string>

#include "GameLogic/Items/ExactLevelLine.h"

using GameLogic::Items::ExactLevelLine::Parse;
using GameLogic::Items::ExactLevelLine::Resolve;

TEST_CASE("The exact level line gives the name and seven levels")
{
    const auto line = Parse(L"#UPL|Hero|ef00e0f");
    REQUIRE(line.has_value());
    CHECK(line->name == L"Hero");
    CHECK(line->any);
    CHECK(line->levels[0] == 14);
    CHECK(line->levels[1] == 15);
    CHECK(line->levels[2] == 0);
    CHECK(line->levels[4] == 14);
    CHECK(line->levels[6] == 15);

    const auto none = Parse(L"#UPL|Hero|0000000\r\n");
    REQUIRE(none.has_value());
    CHECK_FALSE(none->any);
}

TEST_CASE("The exact level line refuses bad lines")
{
    CHECK_FALSE(Parse(L"").has_value());
    CHECK_FALSE(Parse(L"#UPL|").has_value());
    CHECK_FALSE(Parse(L"#UPL||e000000").has_value());
    CHECK_FALSE(Parse(L"#UPL|Hero").has_value());
    CHECK_FALSE(Parse(L"#UPL|Hero|e00000").has_value());
    CHECK_FALSE(Parse(L"#UPL|Hero|e0000000").has_value());
    CHECK_FALSE(Parse(L"#UPL|Hero|d000000").has_value());
    CHECK_FALSE(Parse(L"#UPL|Hero|E000000").has_value());
    CHECK_FALSE(Parse(L"#ANS|Hero|e000000").has_value());
}

TEST_CASE("The exact level only replaces the level the look packet gave for it")
{
    CHECK(Resolve(13, 14) == 14); // +14 arrives as 13
    CHECK(Resolve(15, 15) == 15);
    CHECK(Resolve(13, 0) == 13);  // a real +13
    CHECK(Resolve(11, 14) == 11); // the item changed since the line
    CHECK(Resolve(15, 14) == 15);
    CHECK(Resolve(0, 14) == 0);
}
