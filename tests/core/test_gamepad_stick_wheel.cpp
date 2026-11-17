#include <doctest.h>

#include "Core/Input/GamepadStickWheel.h"

#include <cstdlib>

namespace Pad = Core::Input::Gamepad;
using Pad::StickPosition;
using Pad::StickWheel;

namespace
{
constexpr float Frame = 1.f / 60.f;

// The notches turned over `seconds` held at `stick`, frames of `frame` each, after the first.
int TurnedOver(StickWheel& wheel, StickPosition stick, float seconds, float frame)
{
    int turned = 0;
    for (float passed = 0.f; passed < seconds - 1e-4f; passed += frame)
        turned += wheel.Take(stick, true, frame);
    return turned;
}
} // namespace

TEST_CASE("a soft push or one off a scrolling place moves the pointer, no wheel [core][gamepad]")
{
    StickWheel wheel;
    CHECK(wheel.Take({0.f, 0.5f}, true, Frame) == 0);
    CHECK_FALSE(wheel.Turning());
    CHECK(wheel.Take({0.f, 1.f}, false, Frame) == 0);
    CHECK_FALSE(wheel.Turning());
    // More to the side than up or down: across the window.
    CHECK(wheel.Take({0.8f, 0.7f}, true, Frame) == 0);
    CHECK_FALSE(wheel.Turning());
}

TEST_CASE("a hard push turns one notch at once, down for down and up for up [core][gamepad]")
{
    StickWheel wheel;
    CHECK(wheel.Take({0.f, 0.7f}, true, Frame) == -1);
    CHECK(wheel.Turning());
    wheel.Reset();
    CHECK(wheel.Take({0.f, -0.7f}, true, Frame) == 1);
}

TEST_CASE("the pace grows with the push and goes by the clock, not the frames [core][gamepad]")
{
    StickWheel soft;
    soft.Take({0.f, 0.6f}, true, Frame);
    const int softTurned = TurnedOver(soft, {0.f, 0.6f}, 1.f, Frame);
    StickWheel hard;
    hard.Take({0.f, 1.f}, true, Frame);
    const int hardTurned = TurnedOver(hard, {0.f, 1.f}, 1.f, Frame);
    CHECK(softTurned == doctest::Approx(-StickWheel::SlowestPace).epsilon(0.2));
    CHECK(hardTurned == doctest::Approx(-StickWheel::FastestPace).epsilon(0.1));

    // The same second at 30 and at 144 frames a second turns about the same.
    StickWheel slow;
    slow.Take({0.f, 0.8f}, true, 1.f / 30.f);
    StickWheel fast;
    fast.Take({0.f, 0.8f}, true, 1.f / 144.f);
    const int at30 = TurnedOver(slow, {0.f, 0.8f}, 1.f, 1.f / 30.f);
    const int at144 = TurnedOver(fast, {0.f, 0.8f}, 1.f, 1.f / 144.f);
    CHECK(std::abs(at30 - at144) <= 1);
}

TEST_CASE("it goes on down to the stop push, and a long frame turns a few notches at most [core][gamepad]")
{
    StickWheel wheel;
    wheel.Take({0.f, 0.65f}, true, Frame);
    // Under the start, over the stop: still turning.
    wheel.Take({0.f, 0.55f}, true, Frame);
    CHECK(wheel.Turning());
    CHECK(wheel.Take({0.f, 0.45f}, true, Frame) == 0);
    CHECK_FALSE(wheel.Turning());

    wheel.Take({0.f, 1.f}, true, Frame);
    CHECK(wheel.Take({0.f, 1.f}, true, 2.f) == -StickWheel::MostSteps);
}

TEST_CASE("moved off the scrolling place, it stops and starts afresh [core][gamepad]")
{
    StickWheel wheel;
    wheel.Take({0.f, 1.f}, true, Frame);
    CHECK(wheel.Take({0.f, 1.f}, false, Frame) == 0);
    CHECK_FALSE(wheel.Turning());
    CHECK(wheel.Take({0.f, 1.f}, true, Frame) == -1);
}
