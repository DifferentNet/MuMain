#include "Core/Input/GamepadStickClick.h"

namespace Core::Input::Gamepad
{
namespace
{
// A stick leaning further than this past its dead zone (near its rim) is being pushed hard, not
// clicked. Aiming the pointer leans it less.
constexpr float SwingLean = 0.75f;
// How long the stick must have stood near its centre before a click counts: two or three frames of a
// slow phone.
constexpr auto CalmTime = std::chrono::milliseconds(120);
} // namespace

void StickClick::Update(float lean, bool pressed, bool held, Clock::time_point now)
{
    if (lean > SwingLean)
    {
        m_Swung = true;
        m_LastSwing = now;
        // A held press swung out again is a thumb pushing the stick, not a click after an aim.
        m_Waiting = false;
    }
    m_HeldNow = held;
    m_Pressed = false;
    const bool calm = !m_Swung || now - m_LastSwing >= CalmTime;
    if (pressed)
    {
        m_Accepted = calm;
        m_Pressed = calm;
        m_Waiting = !calm && held && lean <= SwingLean;
    }
    else if (!held)
    {
        m_Accepted = false;
        m_Waiting = false;
    }
    else if (m_Waiting && calm)
    {
        m_Waiting = false;
        m_Accepted = true;
        m_Pressed = true;
    }
}
} // namespace Core::Input::Gamepad
