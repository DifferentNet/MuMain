#include <doctest.h>

#include "Core/Input/GamepadRumble.h"

#include <chrono>

namespace Pad = Core::Input::Gamepad;

namespace
{
using Clock = Pad::RumbleArbiter::Clock;
using std::chrono::milliseconds;

// A hard blow's shake and a target pick's light tick.
constexpr float BlowLow = 0.8f;
constexpr float BlowHigh = 0.9f;
constexpr int BlowMilliseconds = 160;
constexpr float TickHigh = 0.22f;
constexpr int TickMilliseconds = 40;
} // namespace

TEST_CASE("a light tick during a hard blow's shake is dropped [core][gamepad]")
{
    Pad::RumbleArbiter arbiter;
    const Clock::time_point start = Clock::now();
    CHECK(arbiter.Take(BlowLow, BlowHigh, BlowMilliseconds, start));
    CHECK_FALSE(arbiter.Take(0.f, TickHigh, TickMilliseconds, start + milliseconds(50)));
}

TEST_CASE("once the shake ended, a light tick plays [core][gamepad]")
{
    Pad::RumbleArbiter arbiter;
    const Clock::time_point start = Clock::now();
    CHECK(arbiter.Take(BlowLow, BlowHigh, BlowMilliseconds, start));
    CHECK(arbiter.Take(0.f, TickHigh, TickMilliseconds, start + milliseconds(BlowMilliseconds)));
}

TEST_CASE("a stronger shake takes over a weaker one, an equal one too [core][gamepad]")
{
    Pad::RumbleArbiter arbiter;
    const Clock::time_point start = Clock::now();
    CHECK(arbiter.Take(0.f, TickHigh, TickMilliseconds, start));
    CHECK(arbiter.Take(BlowLow, BlowHigh, BlowMilliseconds, start + milliseconds(10)));
    CHECK(arbiter.Take(BlowHigh, BlowLow, BlowMilliseconds, start + milliseconds(20)));
    // The weaker tick is still dropped under the new shake (it would end before it).
    CHECK_FALSE(arbiter.Take(0.f, TickHigh, TickMilliseconds, start + milliseconds(100)));
}

TEST_CASE("a weaker shake that runs on past the stronger one plays [core][gamepad]")
{
    Pad::RumbleArbiter arbiter;
    const Clock::time_point start = Clock::now();
    CHECK(arbiter.Take(BlowLow, BlowHigh, BlowMilliseconds, start));
    // A level up's long soft swell right after a blow.
    CHECK(arbiter.Take(0.3f, 0.2f, 600, start + milliseconds(20)));
    // It is the one under way now: a tick within it is dropped.
    CHECK_FALSE(arbiter.Take(0.f, TickHigh, TickMilliseconds, start + milliseconds(100)));
}

TEST_CASE("a shake allowed but not recorded (the pad refused it) leaves nothing under way [core][gamepad]")
{
    Pad::RumbleArbiter arbiter;
    const Clock::time_point start = Clock::now();
    CHECK(arbiter.Allows(BlowLow, BlowHigh, BlowMilliseconds, start));
    CHECK(arbiter.Take(0.f, TickHigh, TickMilliseconds, start + milliseconds(10)));
}
