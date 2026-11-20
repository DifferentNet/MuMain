#include "Core/Input/GamepadBattery.h"

namespace Core::Input::Gamepad
{
std::optional<int> BatteryWatch::Note(int percent, bool onBattery)
{
    if (IsFine(percent, onBattery))
    {
        m_Said = 0;
        return std::nullopt;
    }
    if (percent < 0)
        return std::nullopt;
    const int line = percent <= VeryLowPercent ? 2 : (percent <= LowPercent ? 1 : 0);
    if (line <= m_Said)
        return std::nullopt;
    m_Said = line;
    return percent;
}

bool BatteryWatch::IsFine(int percent, bool onBattery)
{
    return !onBattery || percent > ReArmPercent;
}

void BatteryWatch::Reset()
{
    m_Said = 0;
}
} // namespace Core::Input::Gamepad
