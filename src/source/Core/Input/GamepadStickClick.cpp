#include "Core/Input/GamepadStickClick.h"

namespace Core::Input::Gamepad
{
namespace
{
// A stick leaning further than this past its dead zone (about half its whole throw) is being pushed,
// not clicked.
constexpr float SwingLean = 0.35f;
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
    }
    m_HeldNow = held;
    m_Pressed = false;
    if (pressed)
    {
        const bool calm = !m_Swung || now - m_LastSwing >= CalmTime;
        m_Accepted = calm;
        m_Pressed = calm;
    }
    else if (!held)
    {
        m_Accepted = false;
    }
}
} // namespace Core::Input::Gamepad
