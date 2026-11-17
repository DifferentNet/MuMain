#include "Core/Input/GamepadStickWheel.h"

#include <algorithm>
#include <cmath>

namespace Core::Input::Gamepad
{
bool StickWheel::MayTurn(StickPosition stick) const
{
    const float upDown = std::fabs(stick.y);
    // More up or down than to the side: a diagonal push moves the pointer across the window.
    if (upDown <= std::fabs(stick.x))
        return false;
    return upDown >= (m_Turning ? StopLean : StartLean);
}

int StickWheel::Take(StickPosition stick, bool overScroll, float seconds)
{
    if (!overScroll || !MayTurn(stick))
    {
        Reset();
        return 0;
    }
    const float upDown = std::fabs(stick.y);
    if (!m_Turning)
    {
        m_Turning = true;
        m_Owed = 1.f;
    }
    else
    {
        // From the start push to the rim the pace grows from the slowest to the fastest; under the
        // start (on the way out) it keeps the slowest.
        const float share = std::clamp((upDown - StartLean) / (1.f - StartLean), 0.f, 1.f);
        m_Owed += (SlowestPace + (FastestPace - SlowestPace) * share) * std::max(seconds, 0.f);
    }
    const int steps = std::min(static_cast<int>(m_Owed), MostSteps);
    m_Owed = std::min(m_Owed - static_cast<float>(steps), 1.f);
    // The stick down (y positive) shows what is below: the wheel turned down.
    return stick.y > 0.f ? -steps : steps;
}

void StickWheel::Reset()
{
    m_Turning = false;
    m_Owed = 0.f;
}
} // namespace Core::Input::Gamepad
