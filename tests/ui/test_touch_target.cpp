#include <doctest.h>

#include "UI/Mobile/TouchTarget.h"

#include <array>

using UI::Mobile::TouchTarget::Box;
using UI::Mobile::TouchTarget::Closest;
using UI::Mobile::TouchTarget::DistanceOutside;
using UI::Mobile::TouchTarget::NoBox;

namespace
{
// Two buttons side by side, 10 pixels apart: 0..40 and 50..90 across, 0..40 down.
constexpr Box Left{true, 0.f, 0.f, 40.f, 40.f};
constexpr Box Right{true, 50.f, 0.f, 90.f, 40.f};
constexpr float Slop = 12.f;
} // namespace

TEST_CASE("the distance outside a box is zero inside it and on its edge")
{
    CHECK(DistanceOutside(Left, 20.f, 20.f) == 0.f);
    CHECK(DistanceOutside(Left, 40.f, 40.f) == 0.f);
    CHECK(DistanceOutside(Left, 43.f, 20.f) == doctest::Approx(3.f));
    CHECK(DistanceOutside(Left, 20.f, -4.f) == doctest::Approx(4.f));
    // Past a corner it is the way to the corner.
    CHECK(DistanceOutside(Left, 43.f, 44.f) == doctest::Approx(5.f));
}

TEST_CASE("a finger inside a button takes it, even within the margin of one preferred before it")
{
    const std::array boxes{Left, Right};
    // 52 is inside the right button and 12 past the left one's edge.
    CHECK(Closest(boxes, 52.f, 20.f, Slop) == 1);
    CHECK(Closest(boxes, 38.f, 20.f, Slop) == 0);
}

TEST_CASE("between buttons the closer one takes the finger, and the first of equals")
{
    const std::array boxes{Left, Right};
    CHECK(Closest(boxes, 43.f, 20.f, Slop) == 0);
    CHECK(Closest(boxes, 47.f, 20.f, Slop) == 1);
    CHECK(Closest(boxes, 45.f, 20.f, Slop) == 0);
}

TEST_CASE("a finger beyond every margin, or only over hidden boxes, takes nothing")
{
    const std::array boxes{Left, Right};
    CHECK(Closest(boxes, 120.f, 20.f, Slop) == NoBox);
    CHECK(Closest(boxes, 20.f, 60.f, Slop) == NoBox);
    CHECK(Closest(boxes, 43.f, 20.f, 0.f) == NoBox);

    const std::array hidden{Box{}, Right};
    CHECK(Closest(hidden, 20.f, 20.f, Slop) == NoBox);
    CHECK(Closest(hidden, 43.f, 20.f, Slop) == 1);
}

TEST_CASE("overlapping boxes go to the first that holds the finger")
{
    const std::array boxes{Right, Box{true, 30.f, 0.f, 70.f, 40.f}};
    CHECK(Closest(boxes, 60.f, 20.f, Slop) == 0);
    CHECK(Closest(boxes, 35.f, 20.f, Slop) == 1);
}
