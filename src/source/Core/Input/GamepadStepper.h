// The d-pad and the left stick stepping through a grid of choices (the pad's window menu, its
// on-screen keyboard): one step as soon as they point somewhere, another after a pause while held,
// then one at a steady pace. A d-pad tap that began and ended between two frames (a slow phone)
// still steps.
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

class Stepper
{
public:
    using Clock = std::chrono::steady_clock;

    Stepper(Clock::duration firstRepeat, Clock::duration repeat);

    // Once a frame: the step to take now, or none.
    Step Take(const Presses& presses, Clock::time_point now);
    // Forgets the way it pointed: the next lean steps at once.
    void Reset();

private:
    Clock::duration m_FirstRepeat;
    Clock::duration m_Repeat;
    Step m_Pointing;
    Clock::time_point m_Next{};
};

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
