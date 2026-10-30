#include <doctest.h>

#include "Core/Input/GamepadStick.h"
#include "Core/Input/GamepadStickClick.h"

#include <chrono>

namespace Pad = Core::Input::Gamepad;

namespace
{
using Clock = Pad::StickClick::Clock;
using std::chrono::milliseconds;

// A frame of a slow phone.
constexpr milliseconds SlowFrame{50};
// The stick's lean past its dead zone: at rest, a slight lean (aiming the pointer), at its rim.
constexpr float Rest = 0.f;
constexpr float Aiming = 0.3f;
constexpr float Rim = 1.f;
} // namespace

TEST_CASE("a stick inside its dead zone rests, past it the lean grows from 0 to 1 [core][gamepad]")
{
    const Pad::StickPosition drift = Pad::PastDeadZone(0.2f, 0.1f);
    CHECK(drift.x == 0.f);
    CHECK(drift.y == 0.f);

    const Pad::StickPosition edge = Pad::PastDeadZone(Pad::StickDeadZone + 0.01f, 0.f);
    CHECK(edge.x > 0.f);
    CHECK(edge.x < 0.02f);

    const Pad::StickPosition rim = Pad::PastDeadZone(0.f, -1.f);
    CHECK(rim.y == doctest::Approx(-1.f));

    // Round: a full diagonal is a full lean, the way unchanged.
    const Pad::StickPosition diagonal = Pad::PastDeadZone(0.8f, 0.8f);
    CHECK(Pad::Lean(diagonal) == doctest::Approx(1.f));
    CHECK(diagonal.x == doctest::Approx(diagonal.y));
}

TEST_CASE("a push starts further out than it ends, so a stick resting at the edge does not walk [core][gamepad]")
{
    // A worn stick resting at 0.3 of its throw: past the dead zone, but no push.
    const Pad::StickPosition resting = Pad::PastDeadZone(0.3f, 0.f);
    CHECK(Pad::Lean(resting) > 0.f);
    CHECK_FALSE(Pad::IsPushed(resting, false));

    const Pad::StickPosition pushed = Pad::PastDeadZone(0.4f, 0.f);
    CHECK(Pad::IsPushed(pushed, false));
    // Let back towards its centre, it goes on until it rests near the edge.
    CHECK(Pad::IsPushed(Pad::PastDeadZone(0.31f, 0.f), true));
    CHECK_FALSE(Pad::IsPushed(Pad::PastDeadZone(0.27f, 0.f), true));
    CHECK_FALSE(Pad::IsPushed({}, true));
}

TEST_CASE("a stick click counts with the stick near its centre [core][gamepad]")
{
    Pad::StickClick click;
    const Clock::time_point start = Clock::now();

    click.Update(Rest, true, true, start);
    CHECK(click.Pressed());
    CHECK(click.Held());

    click.Update(Rest, false, true, start + SlowFrame);
    CHECK_FALSE(click.Pressed());
    CHECK(click.Held());

    click.Update(Rest, false, false, start + 2 * SlowFrame);
    CHECK_FALSE(click.Held());
}

TEST_CASE("a click shorter than a frame still counts once [core][gamepad]")
{
    Pad::StickClick click;
    const Clock::time_point start = Clock::now();

    click.Update(Rest, true, false, start);
    CHECK(click.Pressed());
    CHECK_FALSE(click.Held());
    click.Update(Rest, false, false, start + SlowFrame);
    CHECK_FALSE(click.Pressed());
}

TEST_CASE("a stick pushed to its rim clicks by accident: ignored until let go [core][gamepad]")
{
    Pad::StickClick click;
    const Clock::time_point start = Clock::now();

    click.Update(Rim, true, true, start);
    CHECK_FALSE(click.Pressed());
    CHECK_FALSE(click.Held());

    // Back at its centre with the button still down: still not meant.
    click.Update(Rest, false, true, start + 4 * SlowFrame);
    CHECK_FALSE(click.Held());
}

TEST_CASE("a click right after the stick swung back from its rim is ignored, a later one counts [core][gamepad]")
{
    Pad::StickClick click;
    const Clock::time_point start = Clock::now();

    click.Update(Rim, false, false, start);
    // The thumb let the stick spring back and the click came within the next slow frame.
    click.Update(Rest, true, true, start + SlowFrame);
    CHECK_FALSE(click.Pressed());
    click.Update(Rest, false, false, start + 2 * SlowFrame);

    click.Update(Aiming, true, true, start + 4 * SlowFrame);
    CHECK(click.Pressed());
    CHECK(click.Held());
}
