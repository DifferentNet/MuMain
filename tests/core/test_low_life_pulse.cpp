#include <doctest.h>

#include "Core/Input/LowLifePulse.h"

#include <chrono>

namespace Pad = Core::Input::Gamepad;

namespace
{
using Pulse = Pad::LowLifePulse::Pulse;
using Clock = Pad::LowLifePulse::Clock;
using std::chrono::milliseconds;
using std::chrono::seconds;

// A watch that has seen full life once, at `start`.
Pad::LowLifePulse Watching(Clock::time_point start)
{
    Pad::LowLifePulse watch;
    // The first reading is no fall.
    CHECK(watch.Note(1.f, false, start) == Pad::LowLifePulse::Pulse::None);
    return watch;
}
} // namespace

TEST_CASE("life falling to a quarter pulses twice, 180 ms apart [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch = Watching(start);
    CHECK(watch.Note(0.5f, false, start + milliseconds(10)) == Pulse::None);
    CHECK(watch.Note(0.25f, false, start + milliseconds(20)) == Pulse::First);
    CHECK(watch.Note(0.25f, false, start + milliseconds(100)) == Pulse::None);
    CHECK(watch.Note(0.24f, false, start + milliseconds(199)) == Pulse::None);
    CHECK(watch.Note(0.24f, false, start + milliseconds(200)) == Pulse::Second);
    CHECK(watch.Note(0.2f, false, start + milliseconds(300)) == Pulse::None);
}

TEST_CASE("a blow shaking the pad as life crosses holds the first beat until its shake ended [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch = Watching(start);
    // The blow that crosses the line shakes the pad until start + 240 ms (its 160 ms and a gap).
    const Clock::time_point blowEnds = start + milliseconds(240);
    CHECK(watch.Note(0.2f, false, start, blowEnds) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + milliseconds(100)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + milliseconds(239)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + milliseconds(250)) == Pulse::First);
    // The second 180 ms after the first beat, not after the fall.
    CHECK(watch.Note(0.2f, false, start + milliseconds(420)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + milliseconds(430)) == Pulse::Second);
    CHECK(watch.Note(0.2f, false, start + milliseconds(700)) == Pulse::None);

    // A shake that already ended is no wait.
    Pad::LowLifePulse late = Watching(start);
    CHECK(late.Note(0.2f, false, start + milliseconds(10), start) == Pulse::First);

    // A death while the first beat waits drops the pair.
    Pad::LowLifePulse dying = Watching(start);
    CHECK(dying.Note(0.2f, false, start, blowEnds) == Pulse::None);
    CHECK(dying.Note(0.f, true, start + milliseconds(100)) == Pulse::None);
    CHECK(dying.Note(0.f, true, start + milliseconds(300)) == Pulse::None);
}

TEST_CASE("life staying above a quarter never pulses [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch = Watching(start);
    CHECK(watch.Note(0.26f, false, start + milliseconds(10)) == Pulse::None);
    CHECK(watch.Note(0.9f, false, start + milliseconds(20)) == Pulse::None);
    CHECK(watch.Note(0.251f, false, start + milliseconds(30)) == Pulse::None);
}

TEST_CASE("the next pair waits until life was back at 40 % [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch = Watching(start);
    CHECK(watch.Note(0.2f, false, start) == Pulse::First);
    CHECK(watch.Note(0.2f, false, start + milliseconds(180)) == Pulse::Second);
    // Healed to 39 %: falling again is not felt.
    CHECK(watch.Note(0.39f, false, start + seconds(20)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + seconds(21)) == Pulse::None);
    // Healed to 40 %: the next fall is.
    CHECK(watch.Note(0.4f, false, start + seconds(22)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + seconds(23)) == Pulse::First);
}

TEST_CASE("a pair comes at most once every 10 seconds [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch = Watching(start);
    CHECK(watch.Note(0.2f, false, start) == Pulse::First);
    CHECK(watch.Note(0.2f, false, start + milliseconds(180)) == Pulse::Second);
    CHECK(watch.Note(0.5f, false, start + seconds(2)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + seconds(3)) == Pulse::None);
    // Still armed: the first fall after the gap pulses.
    CHECK(watch.Note(0.3f, false, start + seconds(11)) == Pulse::None);
    CHECK(watch.Note(0.2f, false, start + seconds(12)) == Pulse::First);
}

TEST_CASE("no pulse while dead, and a death cancels the second [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch = Watching(start);
    CHECK(watch.Note(0.f, true, start + milliseconds(10)) == Pulse::None);

    Pad::LowLifePulse hurt = Watching(start);
    CHECK(hurt.Note(0.2f, false, start) == Pulse::First);
    CHECK(hurt.Note(0.f, true, start + milliseconds(50)) == Pulse::None);
    CHECK(hurt.Note(0.f, true, start + milliseconds(200)) == Pulse::None);
    // Back to life in full, then hit hard again after the gap: felt anew.
    CHECK(hurt.Note(1.f, false, start + seconds(15)) == Pulse::None);
    CHECK(hurt.Note(0.1f, false, start + seconds(16)) == Pulse::First);
}

TEST_CASE("a first reading never pulses, hurt or not [core][gamepad]")
{
    const Clock::time_point start = Clock::now();
    Pad::LowLifePulse watch;
    CHECK(watch.Note(0.1f, false, start) == Pulse::None);
    CHECK(watch.Note(0.05f, false, start + milliseconds(10)) == Pulse::None);

    // Forgotten (the pad put down) and read again low: still no pulse.
    Pad::LowLifePulse forgotten = Watching(start);
    forgotten.Forget();
    CHECK(forgotten.Note(0.2f, false, start + milliseconds(10)) == Pulse::None);
}
