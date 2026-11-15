#include "Core/Input/GamepadRumble.h"

#include <algorithm>

namespace Core::Input::Gamepad
{
namespace
{
// The stronger motor says how hard a shake is felt.
float Strength(float low, float high)
{
    return std::max(low, high);
}
} // namespace

bool RumbleArbiter::Allows(float low, float high, int milliseconds, Clock::time_point now) const
{
    if (now >= m_Until || Strength(low, high) >= m_Strength)
        return true;
    return now + std::chrono::milliseconds(milliseconds) > m_Until;
}

void RumbleArbiter::Record(float low, float high, int milliseconds, Clock::time_point now)
{
    m_Strength = Strength(low, high);
    m_Until = now + std::chrono::milliseconds(milliseconds);
}

bool RumbleArbiter::Take(float low, float high, int milliseconds, Clock::time_point now)
{
    if (!Allows(low, high, milliseconds, now))
        return false;
    Record(low, high, milliseconds, now);
    return true;
}
} // namespace Core::Input::Gamepad
