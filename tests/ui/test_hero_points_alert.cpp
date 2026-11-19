#include <doctest.h>

#include "UI/Mobile/HeroFrame.h"

using UI::Mobile::HeroFrame::PointsToSpend;

TEST_CASE("the hero frame's plus shows for stat points [ui][hero_frame]")
{
    CHECK(PointsToSpend(5, false, 0));
    CHECK(PointsToSpend(5, true, 0));
    CHECK_FALSE(PointsToSpend(0, false, 0));
}

TEST_CASE("the hero frame's plus shows for master points on a master only [ui][hero_frame]")
{
    CHECK(PointsToSpend(0, true, 3));
    CHECK_FALSE(PointsToSpend(0, true, 0));
    // Master points a character below master level cannot spend (stale data) show nothing.
    CHECK_FALSE(PointsToSpend(0, false, 3));
    CHECK_FALSE(PointsToSpend(0, true, -1));
}
