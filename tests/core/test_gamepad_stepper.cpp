#include <doctest.h>

#include "Core/Input/GamepadStepper.h"

#include <chrono>

namespace Pad = Core::Input::Gamepad;

namespace
{
using Clock = Pad::Stepper::Clock;
using std::chrono::milliseconds;

constexpr milliseconds Frame{16};
constexpr milliseconds FirstRepeat{300};
constexpr milliseconds Repeat{100};
// Past the stepper's long hold, when the pace quickens.
constexpr milliseconds LongHold{1300};

const Pad::Presses NoPresses{};

Pad::Aim Stick(float x, float y)
{
    Pad::Aim aim;
    aim.stick = {x, y};
    return aim;
}

bool IsStep(Pad::Step step, int dx, int dy)
{
    return step.dx == dx && step.dy == dy;
}

bool IsStill(Pad::Step step)
{
    return IsStep(step, 0, 0);
}
} // namespace

TEST_CASE("the stick leans in further out than it leans out [core][gamepad]")
{
    CHECK(IsStill(Pad::PointingOf(Stick(0.55f, 0.f), {})));
    CHECK(IsStep(Pad::PointingOf(Stick(0.65f, 0.f), {}), 1, 0));
    // Once pointing, it holds on down to 0.4.
    CHECK(IsStep(Pad::PointingOf(Stick(0.45f, 0.f), {1, 0}), 1, 0));
    CHECK(IsStill(Pad::PointingOf(Stick(0.35f, 0.f), {1, 0})));
}

TEST_CASE("the stick keeps its axis until the other is clearly bigger [core][gamepad]")
{
    CHECK(IsStep(Pad::PointingOf(Stick(0.7f, 0.75f), {1, 0}), 1, 0));
    CHECK(IsStep(Pad::PointingOf(Stick(0.75f, 0.7f), {0, 1}), 0, 1));
    CHECK(IsStep(Pad::PointingOf(Stick(0.5f, -0.7f), {1, 0}), 0, -1));
    // From rest the bigger axis wins.
    CHECK(IsStep(Pad::PointingOf(Stick(-0.6f, 0.7f), {}), 0, 1));
}

TEST_CASE("the d-pad wins over the stick [core][gamepad]")
{
    Pad::Aim aim = Stick(0.9f, 0.f);
    aim.dpad = {0, -1};
    CHECK(IsStep(Pad::PointingOf(aim, {1, 0}), 0, -1));
}

TEST_CASE("a stick held near a diagonal steps once, not on its noise [core][gamepad]")
{
    Pad::Stepper stepper{FirstRepeat, Repeat};
    const Clock::time_point start = Clock::now();
    int steps = 0;
    for (int frame = 0; frame < 15; ++frame)
    {
        const float noise = frame % 2 == 0 ? 0.02f : -0.02f;
        const Pad::Step step = stepper.Take(Stick(0.7f + noise, 0.7f - noise), NoPresses, start + frame * Frame);
        if (!IsStill(step))
            ++steps;
    }
    CHECK(steps == 1);
}

TEST_CASE("a stick flickering around the lean steps once [core][gamepad]")
{
    Pad::Stepper stepper{FirstRepeat, Repeat};
    const Clock::time_point start = Clock::now();
    int steps = 0;
    for (int frame = 0; frame < 15; ++frame)
    {
        const float lean = frame % 2 == 0 ? 0.62f : 0.48f;
        if (!IsStill(stepper.Take(Stick(lean, 0.f), NoPresses, start + frame * Frame)))
            ++steps;
    }
    CHECK(steps == 1);
}

TEST_CASE("a held stick steps, waits, then repeats, quicker after a long hold [core][gamepad]")
{
    Pad::Stepper stepper{FirstRepeat, Repeat};
    const Clock::time_point start = Clock::now();
    const Pad::Aim right = Stick(0.9f, 0.f);

    CHECK(IsStep(stepper.Take(right, NoPresses, start), 1, 0));
    CHECK(IsStill(stepper.Take(right, NoPresses, start + Frame)));
    CHECK(IsStep(stepper.Take(right, NoPresses, start + FirstRepeat), 1, 0));
    CHECK(IsStill(stepper.Take(right, NoPresses, start + FirstRepeat + Repeat / 2)));
    CHECK(IsStep(stepper.Take(right, NoPresses, start + FirstRepeat + Repeat), 1, 0));

    // Past the long hold: the next waits less than the pace of before.
    CHECK(IsStep(stepper.Take(right, NoPresses, start + LongHold), 1, 0));
    CHECK(IsStep(stepper.Take(right, NoPresses, start + LongHold + milliseconds(65)), 1, 0));
}

TEST_CASE("a held d-pad keeps its pace after a long hold [core][gamepad]")
{
    Pad::Stepper stepper{FirstRepeat, Repeat};
    const Clock::time_point start = Clock::now();
    Pad::Aim down;
    down.dpad = {0, 1};

    CHECK(IsStep(stepper.Take(down, NoPresses, start), 0, 1));
    CHECK(IsStep(stepper.Take(down, NoPresses, start + LongHold), 0, 1));
    CHECK(IsStill(stepper.Take(down, NoPresses, start + LongHold + milliseconds(65))));
    CHECK(IsStep(stepper.Take(down, NoPresses, start + LongHold + Repeat), 0, 1));
}

TEST_CASE("a d-pad tap between two frames still steps [core][gamepad]")
{
    Pad::Stepper stepper{FirstRepeat, Repeat};
    Pad::Presses presses{};
    presses[static_cast<int>(Pad::Button::DpadDown)] = true;
    CHECK(IsStep(stepper.Take(Pad::Aim{}, presses, Clock::now()), 0, 1));
}
