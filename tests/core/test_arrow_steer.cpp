#include <doctest.h>

#include "Core/Input/ArrowSteer.h"

#include <cmath>

namespace Arrows = Core::Input::ArrowSteer;

TEST_CASE("no arrow held walks nowhere [core][input]")
{
    const Arrows::Direction none = Arrows::FromKeys(false, false, false, false);
    CHECK(none.x == 0.f);
    CHECK(none.y == 0.f);
    CHECK_FALSE(Arrows::Any(none));
}

TEST_CASE("each arrow walks its way on the screen, up is y negative [core][input]")
{
    const Arrows::Direction left = Arrows::FromKeys(true, false, false, false);
    CHECK(left.x == -1.f);
    CHECK(left.y == 0.f);
    const Arrows::Direction right = Arrows::FromKeys(false, true, false, false);
    CHECK(right.x == 1.f);
    CHECK(right.y == 0.f);
    const Arrows::Direction up = Arrows::FromKeys(false, false, true, false);
    CHECK(up.x == 0.f);
    CHECK(up.y == -1.f);
    const Arrows::Direction down = Arrows::FromKeys(false, false, false, true);
    CHECK(down.x == 0.f);
    CHECK(down.y == 1.f);
    CHECK(Arrows::Any(up));
}

TEST_CASE("opposite arrows cancel [core][input]")
{
    CHECK_FALSE(Arrows::Any(Arrows::FromKeys(true, true, false, false)));
    CHECK_FALSE(Arrows::Any(Arrows::FromKeys(false, false, true, true)));
    CHECK_FALSE(Arrows::Any(Arrows::FromKeys(true, true, true, true)));
    // Left and right cancel, up still walks.
    const Arrows::Direction up = Arrows::FromKeys(true, true, true, false);
    CHECK(up.x == 0.f);
    CHECK(up.y == -1.f);
}

TEST_CASE("a diagonal is as long as a straight line [core][input]")
{
    const Arrows::Direction upRight = Arrows::FromKeys(false, true, true, false);
    CHECK(upRight.x > 0.f);
    CHECK(upRight.y < 0.f);
    CHECK(std::hypot(upRight.x, upRight.y) == doctest::Approx(1.f));
    CHECK(upRight.x == doctest::Approx(-upRight.y));
    const Arrows::Direction downLeft = Arrows::FromKeys(true, false, false, true);
    CHECK(downLeft.x < 0.f);
    CHECK(downLeft.y > 0.f);
    CHECK(std::hypot(downLeft.x, downLeft.y) == doctest::Approx(1.f));
}
