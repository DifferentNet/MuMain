// The d-pad and the left stick stepping through a grid of choices (the pad's window menu, its
// on-screen keyboard): one step as soon as they point somewhere, another after a pause while held,
// then one at a steady pace, quicker after a long hold (a long list). A d-pad tap that began and
// ended between two frames (a slow phone) still steps.
#pragma once

#include "Core/Input/GamepadInput.h"

#include <array>
#include <chrono>

namespace Core::Input::Gamepad
{
// The buttons pressed since the last frame, by Button.
using Presses = std::array<bool, ButtonCount>;

struct Step
{
    int dx = 0;
    int dy = 0;
};

// Where the d-pad and the left stick point this frame.
struct Aim
{
    // The d-pad held, a step on one axis or none.
    Step dpad;
    StickPosition stick;
};

// The way to step, a step on one axis or none; `previous` is the way of the last frame. The d-pad
// wins. The stick leans in at 0.6 and out at 0.4, and keeps its axis until the other is 1.3 times
// bigger: a stick held near a diagonal or near the lean does not step on its noise.
[[nodiscard]] Step PointingOf(const Aim& aim, Step previous);

class Stepper
{
public:
    using Clock = std::chrono::steady_clock;

    Stepper(Clock::duration firstRepeat, Clock::duration repeat);

    // Once a frame: the step to take now, or none. After a long hold the pace quickens.
    Step Take(const Presses& presses, Clock::time_point now) { return Take(ReadAim(), presses, now); }
    // The same with the pad's state given: the stepping alone, for the tests.
    Step Take(const Aim& aim, const Presses& presses, Clock::time_point now);
    // Forgets the way it pointed: the next lean steps at once.
    void Reset();

private:
    // Inline, so the tests of the stepping need no pad.
    static Aim ReadAim();

    Clock::duration m_FirstRepeat;
    Clock::duration m_Repeat;
    Step m_Pointing;
    Clock::time_point m_HeldSince{};
    Clock::time_point m_Next{};
};

inline Aim Stepper::ReadAim()
{
    Aim aim;
    if (GetButton(Button::DpadLeft).held)
        aim.dpad = {-1, 0};
    else if (GetButton(Button::DpadRight).held)
        aim.dpad = {1, 0};
    else if (GetButton(Button::DpadUp).held)
        aim.dpad = {0, -1};
    else if (GetButton(Button::DpadDown).held)
        aim.dpad = {0, 1};
    aim.stick = LeftStick();
    return aim;
}

// A button that acts when pressed and again and again while held (Backspace, the caret).
class RepeatButton
{
public:
    using Clock = std::chrono::steady_clock;

    RepeatButton(Clock::duration firstRepeat, Clock::duration repeat);

    // Once a frame: whether the button acts now.
    bool Take(bool pressed, bool held, Clock::time_point now);

private:
    Clock::duration m_FirstRepeat;
    Clock::duration m_Repeat;
    Clock::time_point m_Next{};
};
} // namespace Core::Input::Gamepad
