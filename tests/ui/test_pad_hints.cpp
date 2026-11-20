#include <doctest.h>

#include "UI/HUD/PadHintBits.h"

namespace Hints = UI::HUD::PadHints;
using Hints::Hint;

TEST_CASE("each pad hint keeps its bit in GamepadHintsSeen [ui][gamepad]")
{
    // config.ini remembers these values: they never move.
    CHECK(Hints::Marked(0, Hint::PickUp) == 1);
    CHECK(Hints::Marked(0, Hint::Window) == 2);
    CHECK(Hints::Marked(0, Hint::BagFull) == 4);
    CHECK(Hints::Marked(0, Hint::Death) == 8);
    CHECK(Hints::Marked(0, Hint::SkillsThird) == 16);
    CHECK(Hints::Marked(0, Hint::SmallScreen) == 32);
}

TEST_CASE("the small screen bit keeps the hints seen before [ui][gamepad]")
{
    const int before = Hints::Marked(Hints::Marked(0, Hint::PickUp), Hint::SkillsThird);
    const int after = Hints::Marked(before, Hint::SmallScreen);
    CHECK(Hints::Seen(after, Hint::PickUp));
    CHECK(Hints::Seen(after, Hint::SkillsThird));
    CHECK(Hints::Seen(after, Hint::SmallScreen));
    CHECK_FALSE(Hints::Seen(after, Hint::Window));
    CHECK_FALSE(Hints::Seen(before, Hint::SmallScreen));
    // Marked twice is marked once.
    CHECK(Hints::Marked(after, Hint::SmallScreen) == after);
}

TEST_CASE("the small screen hint: up to 800 pixels high [ui][gamepad]")
{
    CHECK(Hints::SmallScreenApplies(800, 100, false));
    CHECK(Hints::SmallScreenApplies(720, 100, false));
    CHECK(Hints::SmallScreenApplies(1, 100, false));
    CHECK_FALSE(Hints::SmallScreenApplies(801, 100, false));
    CHECK_FALSE(Hints::SmallScreenApplies(1080, 100, false));
    // No window size yet.
    CHECK_FALSE(Hints::SmallScreenApplies(0, 100, false));
}

TEST_CASE("the small screen hint: the interface at 100 % or less [ui][gamepad]")
{
    CHECK(Hints::SmallScreenApplies(768, 75, false));
    CHECK(Hints::SmallScreenApplies(768, 90, false));
    CHECK_FALSE(Hints::SmallScreenApplies(768, 125, false));
    CHECK_FALSE(Hints::SmallScreenApplies(768, 150, false));
}

TEST_CASE("the small screen hint: never in the phone view [ui][gamepad]")
{
    CHECK_FALSE(Hints::SmallScreenApplies(390, 100, true));
    CHECK_FALSE(Hints::SmallScreenApplies(800, 75, true));
}
