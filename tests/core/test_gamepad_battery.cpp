#include <doctest.h>

#include "Core/Input/GamepadBattery.h"

namespace Pad = Core::Input::Gamepad;

TEST_CASE("the battery is said once at 20 % and once more at 10 % [core][gamepad]")
{
    Pad::BatteryWatch watch;
    CHECK_FALSE(watch.Note(80, true).has_value());
    CHECK_FALSE(watch.Note(21, true).has_value());
    CHECK(watch.Note(20, true) == 20);
    CHECK_FALSE(watch.Note(19, true).has_value());
    CHECK_FALSE(watch.Note(11, true).has_value());
    CHECK(watch.Note(10, true) == 10);
    CHECK_FALSE(watch.Note(9, true).has_value());
    CHECK_FALSE(watch.Note(0, true).has_value());
}

TEST_CASE("a coarse step past both lines says it once [core][gamepad]")
{
    Pad::BatteryWatch watch;
    CHECK_FALSE(watch.Note(70, true).has_value());
    CHECK(watch.Note(5, true) == 5);
    CHECK_FALSE(watch.Note(5, true).has_value());
}

TEST_CASE("the lines are said again only after the battery was over 25 % [core][gamepad]")
{
    Pad::BatteryWatch watch;
    CHECK(watch.Note(20, true) == 20);
    // Back to 25 %: not yet.
    CHECK_FALSE(watch.Note(25, true).has_value());
    CHECK_FALSE(watch.Note(20, true).has_value());
    // Over 25 %: armed again.
    CHECK_FALSE(watch.Note(26, true).has_value());
    CHECK(watch.Note(20, true) == 20);
}

TEST_CASE("a pad on a cable or charging says nothing, and is armed again [core][gamepad]")
{
    Pad::BatteryWatch watch;
    CHECK_FALSE(watch.Note(5, false).has_value());
    CHECK(watch.Note(15, true) == 15);
    CHECK_FALSE(watch.Note(15, false).has_value());
    CHECK(watch.Note(15, true) == 15);
}

TEST_CASE("a cable or a battery over 25 % is fine again: a line not said yet is stale [core][gamepad]")
{
    CHECK(Pad::BatteryWatch::IsFine(15, false));
    CHECK(Pad::BatteryWatch::IsFine(-1, false));
    CHECK(Pad::BatteryWatch::IsFine(26, true));
    CHECK_FALSE(Pad::BatteryWatch::IsFine(25, true));
    CHECK_FALSE(Pad::BatteryWatch::IsFine(15, true));
    // Unknown on its battery: what was found low stays news.
    CHECK_FALSE(Pad::BatteryWatch::IsFine(-1, true));
}

TEST_CASE("an unknown percent says nothing [core][gamepad]")
{
    Pad::BatteryWatch watch;
    CHECK_FALSE(watch.Note(-1, true).has_value());
    CHECK_FALSE(watch.Note(-1, false).has_value());
    CHECK(watch.Note(20, true) == 20);
    // Unknown in between keeps what was said.
    CHECK_FALSE(watch.Note(-1, true).has_value());
    CHECK_FALSE(watch.Note(18, true).has_value());
}

TEST_CASE("a reset starts afresh [core][gamepad]")
{
    Pad::BatteryWatch watch;
    CHECK(watch.Note(10, true) == 10);
    watch.Reset();
    CHECK(watch.Note(10, true) == 10);
}
