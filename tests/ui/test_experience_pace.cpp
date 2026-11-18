#include <doctest.h>

#include "UI/HUD/ExperiencePace.h"

using UI::HUD::ExperiencePace::MinimumMs;
using UI::HUD::ExperiencePace::MinutesToGain;
using UI::HUD::ExperiencePace::Tracker;
using UI::HUD::ExperiencePace::WindowMs;

TEST_CASE("no pace before a minute of hunting [ui][hud]")
{
    Tracker tracker;
    CHECK_FALSE(tracker.PerHour(0).has_value());
    tracker.Add(1'000, 5'000, false);
    CHECK_FALSE(tracker.PerHour(1'000).has_value());
    CHECK_FALSE(tracker.PerHour(1'000 + MinimumMs - 1).has_value());
    CHECK(tracker.PerHour(1'000 + MinimumMs).has_value());
}

TEST_CASE("pace per hour over the time since the first gain [ui][hud]")
{
    Tracker tracker;
    tracker.Add(0, 1'000, false);
    tracker.Add(60'000, 1'000, false);
    tracker.Add(120'000, 1'000, false);
    // 3,000 in 2 minutes = 90,000 per hour.
    const auto pace = tracker.PerHour(120'000);
    REQUIRE(pace.has_value());
    CHECK(*pace == doctest::Approx(90'000.0));
}

TEST_CASE("only the last 10 minutes count [ui][hud]")
{
    Tracker tracker;
    tracker.Add(0, 100'000, false);
    tracker.Add(WindowMs, 6'000, false);
    // The first gain is older than the window at this time.
    const auto pace = tracker.PerHour(WindowMs + 1);
    REQUIRE(pace.has_value());
    CHECK(*pace == doctest::Approx(6'000.0 * 6.0).epsilon(0.001));

    // Nothing gained for longer than the window: no pace.
    CHECK_FALSE(tracker.PerHour(3 * WindowMs).has_value());
}

TEST_CASE("after a long break the minute starts again [ui][hud]")
{
    Tracker tracker;
    tracker.Add(0, 1'000, false);
    tracker.Add(2 * WindowMs, 1'000, false);
    CHECK_FALSE(tracker.PerHour(2 * WindowMs + 1'000).has_value());
    CHECK(tracker.PerHour(2 * WindowMs + MinimumMs).has_value());
}

TEST_CASE("master and normal experience are counted apart [ui][hud]")
{
    Tracker tracker;
    tracker.Add(0, 50'000, false);
    tracker.Add(30'000, 10, true);
    const auto pace = tracker.PerHour(90'000);
    REQUIRE(pace.has_value());
    CHECK(*pace == doctest::Approx(600.0));
}

TEST_CASE("zero or negative gains are ignored [ui][hud]")
{
    Tracker tracker;
    tracker.Add(0, 0, false);
    tracker.Add(0, -5, false);
    CHECK_FALSE(tracker.PerHour(MinimumMs * 2).has_value());
}

TEST_CASE("minutes to the next level [ui][hud]")
{
    CHECK(MinutesToGain(60'000, 60'000.0) == 60);
    CHECK(MinutesToGain(1, 60'000.0) == 1);
    CHECK(MinutesToGain(1'001, 60'000.0) == 2);
    CHECK_FALSE(MinutesToGain(0, 60'000.0).has_value());
    CHECK_FALSE(MinutesToGain(100, 0.0).has_value());
}
