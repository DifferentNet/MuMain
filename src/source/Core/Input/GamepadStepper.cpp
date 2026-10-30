#include "stdafx.h"
#include "Core/Input/GamepadStepper.h"

#include <algorithm>
#include <cmath>

namespace Core::Input::Gamepad
{
namespace
{
// How far the stick leans before it steps.
constexpr float StickLean = 0.5f;

bool Pressed(const Presses& presses, Button button)
{
    return presses[static_cast<int>(button)];
}

bool IsStill(Step step)
{
    return step.dx == 0 && step.dy == 0;
}

// The way the d-pad or the left stick points, a step on one axis.
Step Pointing()
{
    if (GetButton(Button::DpadLeft).held)
        return {-1, 0};
    if (GetButton(Button::DpadRight).held)
        return {1, 0};
    if (GetButton(Button::DpadUp).held)
        return {0, -1};
    if (GetButton(Button::DpadDown).held)
        return {0, 1};
    const StickPosition stick = LeftStick();
    if (std::max(std::abs(stick.x), std::abs(stick.y)) < StickLean)
        return {};
    if (std::abs(stick.x) >= std::abs(stick.y))
        return {stick.x < 0.f ? -1 : 1, 0};
    return {0, stick.y < 0.f ? -1 : 1};
}

Step Tapped(const Presses& presses)
{
    if (Pressed(presses, Button::DpadLeft))
        return {-1, 0};
    if (Pressed(presses, Button::DpadRight))
        return {1, 0};
    if (Pressed(presses, Button::DpadUp))
        return {0, -1};
    if (Pressed(presses, Button::DpadDown))
        return {0, 1};
    return {};
}
} // namespace

Stepper::Stepper(Clock::duration firstRepeat, Clock::duration repeat) : m_FirstRepeat(firstRepeat), m_Repeat(repeat) {}

Step Stepper::Take(const Presses& presses, Clock::time_point now)
{
    const Step pointing = Pointing();
    const bool turned = pointing.dx != m_Pointing.dx || pointing.dy != m_Pointing.dy;
    m_Pointing = pointing;
    if (IsStill(pointing))
        return Tapped(presses);
    if (turned)
    {
        m_Next = now + m_FirstRepeat;
        return pointing;
    }
    if (now < m_Next)
        return {};
    m_Next = now + m_Repeat;
    return pointing;
}

void Stepper::Reset()
{
    m_Pointing = {};
}

RepeatButton::RepeatButton(Clock::duration firstRepeat, Clock::duration repeat)
    : m_FirstRepeat(firstRepeat), m_Repeat(repeat)
{
}

bool RepeatButton::Take(bool pressed, bool held, Clock::time_point now)
{
    if (pressed)
    {
        m_Next = now + m_FirstRepeat;
        return true;
    }
    if (!held || now < m_Next)
        return false;
    m_Next = now + m_Repeat;
    return true;
}
} // namespace Core::Input::Gamepad
