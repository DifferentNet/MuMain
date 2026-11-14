#include "Core/Input/GamepadRumble.h"

#include <algorithm>

namespace Core::Input::Gamepad
{
bool RumbleArbiter::Take(float low, float high, int milliseconds, Clock::time_point now)
{
    // The stronger motor says how hard a shake is felt.
    const float strength = std::max(low, high);
    if (now < m_Until && strength < m_Strength)
        return false;
    m_Strength = strength;
    m_Until = now + std::chrono::milliseconds(milliseconds);
    return true;
}
} // namespace Core::Input::Gamepad
