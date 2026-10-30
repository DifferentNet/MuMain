#include <doctest.h>

#include "UI/Mobile/CompactNumber.h"

#include <cstdint>

using UI::Mobile::CompactNumber::Format;

TEST_CASE("small numbers are shown as they are [ui][hud]")
{
    CHECK(Format(0) == "0");
    CHECK(Format(7) == "7");
    CHECK(Format(9'999) == "9999");
}

TEST_CASE("numbers from ten thousand are shown in whole thousands [ui][hud]")
{
    CHECK(Format(10'000) == "10k");
    CHECK(Format(10'999) == "10k");
    CHECK(Format(32'767) == "32k");
    CHECK(Format(65'535) == "65k");
    CHECK(Format(999'999) == "999k");
}

TEST_CASE("numbers from a million are shown in whole millions [ui][hud]")
{
    CHECK(Format(1'000'000) == "1M");
    CHECK(Format(1'999'999) == "1M");
    CHECK(Format(UINT32_MAX) == "4294M");
}

TEST_CASE("a compact number never reads more than the value [ui][hud]")
{
    // A full gauge must not look fuller than it is: rounding is always down.
    CHECK(Format(19'999) == "19k");
    CHECK(Format(999'500) == "999k");
}
