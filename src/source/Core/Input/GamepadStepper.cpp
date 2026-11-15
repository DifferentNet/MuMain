#include "Core/Input/GamepadStepper.h"

#include <algorithm>
#include <cmath>

namespace Core::Input::Gamepad
{
namespace
{
// How far the stick leans before it steps, and how far back it goes before it stops.
constexpr float LeanIn = 0.6f;
constexpr float LeanOut = 0.4f;
// How much bigger the other axis must grow before the stick turns to it.
constexpr float AxisTurn = 1.3f;
// After this long a hold of the stick, its pace quickens to FastRepeat (the d-pad keeps its own).
constexpr auto LongHold = std::chrono::milliseconds(1200);
constexpr auto FastRepeat = std::chrono::milliseconds(60);

bool Pressed(const Presses& presses, Button button)
{
    return presses[static_cast<int>(button)];
}

bool IsStill(Step step)
{
    return step.dx == 0 && step.dy == 0;
}

bool OnXAxis(float ax, float ay, Step previous)
{
    if (previous.dx != 0)
        return ay <= ax * AxisTurn;
    if (previous.dy != 0)
        return ax > ay * AxisTurn;
    return ax >= ay;
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

Step PointingOf(const Aim& aim, Step previous)
{
    if (!IsStill(aim.dpad))
        return aim.dpad;
    const float ax = std::abs(aim.stick.x);
    const float ay = std::abs(aim.stick.y);
    const float lean = IsStill(previous) ? LeanIn : LeanOut;
    if (std::max(ax, ay) < lean)
        return {};
    if (OnXAxis(ax, ay, previous))
        return {aim.stick.x < 0.f ? -1 : 1, 0};
    return {0, aim.stick.y < 0.f ? -1 : 1};
}

Stepper::Stepper(Clock::duration firstRepeat, Clock::duration repeat) : m_FirstRepeat(firstRepeat), m_Repeat(repeat) {}

Step Stepper::Take(const Aim& aim, const Presses& presses, Clock::time_point now)
{
    const Step pointing = PointingOf(aim, m_Pointing);
    const bool turned = pointing.dx != m_Pointing.dx || pointing.dy != m_Pointing.dy;
    m_Pointing = pointing;
    m_Repeated = false;
    if (IsStill(pointing))
        return Tapped(presses);
    if (turned)
    {
        m_HeldSince = now;
        m_Next = now + m_FirstRepeat;
        return pointing;
    }
    if (now < m_Next)
        return {};
    const bool longHold = IsStill(aim.dpad) && now - m_HeldSince >= LongHold;
    m_Next = now + (longHold ? std::min<Clock::duration>(m_Repeat, FastRepeat) : m_Repeat);
    m_Repeated = true;
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
