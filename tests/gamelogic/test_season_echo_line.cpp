#include "doctest.h"

#include <string>

#include "GameLogic/Events/SeasonEchoLine.h"

using GameLogic::SeasonEchoLine::Line;
using GameLogic::SeasonEchoLine::Parse;

TEST_CASE("The shown echo line gives the name and the season")
{
    const auto line = Parse(L"#BPE|Ghinzuro|2");
    REQUIRE(line.has_value());
    CHECK(line->name == L"Ghinzuro");
    CHECK(line->season == 2);
    CHECK_FALSE(line->owner);
    CHECK_FALSE(line->hidden);

    CHECK(*Parse(L"#BPE|Bob|0") == Line{L"Bob", 0, false, false});
    CHECK(Parse(L"#BPE|Bob|14\r\n")->season == 14);
    CHECK(Parse(L"#BPE|Bob|3 ")->season == 3);
    CHECK(Parse(L"#BPE|Bob|2147483647")->season == 2147483647);
    CHECK(Parse(L"#BPE|\u0421\u043d\u0435\u0433|2")->name == L"\u0421\u043d\u0435\u0433");
}

TEST_CASE("Season 0 means no echo or a hidden one")
{
    CHECK(Parse(L"#BPE|Bob|0")->season == 0);
    CHECK(Parse(L"#BPE|Bob|0|0")->season == 0);
    CHECK(Parse(L"#BPE|Bob|000")->season == 0);
}

TEST_CASE("The four-field line is the owner line with the owner's switch")
{
    const auto shown = Parse(L"#BPE|Ghinzuro|2|0");
    REQUIRE(shown.has_value());
    CHECK(shown->owner);
    CHECK(shown->season == 2);
    CHECK_FALSE(shown->hidden);

    const auto hidden = Parse(L"#BPE|Ghinzuro|2|1");
    REQUIRE(hidden.has_value());
    CHECK(hidden->owner);
    CHECK(hidden->hidden);

    CHECK(*Parse(L"#BPE|Bob|0|0") == Line{L"Bob", 0, true, false});
    CHECK(*Parse(L"#BPE|Bob|5|1\r\n") == Line{L"Bob", 5, true, true});
}

TEST_CASE("The echo line refuses bad lines")
{
    CHECK_FALSE(Parse(L"").has_value());
    CHECK_FALSE(Parse(L"#BPE|").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|").has_value());
    CHECK_FALSE(Parse(L"#BPE||2").has_value());
    CHECK_FALSE(Parse(L"#BPE||2|0").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|2|").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|2|2").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|2|yes").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|2|0|1").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|-1").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|+1").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob| 2").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|x").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|2147483648").has_value());
    CHECK_FALSE(Parse(L"#BPE|Bob|12345678901").has_value());
    CHECK_FALSE(Parse(L"#BPM|Bob|2").has_value());
    CHECK_FALSE(Parse(L"BPE|Bob|2").has_value());
    CHECK_FALSE(Parse(L"#bpe|Bob|2").has_value());
}
