// When a pad's battery is worth a line in the system log (pad round 7, B1): once as it falls to 20 %
// or less, once more at 10 % or less. It says nothing again until the battery is back over 25 % or the
// pad is no longer on its battery (a cable in, charging). Some pads report coarse steps only (SDL gives
// 100, 70, 20, 5 for them): the watch goes by "at or under", so a step past both lines says it once.
#pragma once

#include <optional>

namespace Core::Input::Gamepad
{
class BatteryWatch
{
public:
    static constexpr int LowPercent = 20;
    static constexpr int VeryLowPercent = 10;
    // Over this the battery counts as charged again.
    static constexpr int ReArmPercent = 25;

    // The pad's battery now: `percent` 0 .. 100, -1 unknown; `onBattery` false when wired or charging.
    // The percent to say now, if any.
    [[nodiscard]] std::optional<int> Note(int percent, bool onBattery);

    // Whether the battery is fine again: on a cable or charging, or back over ReArmPercent. A low line
    // not said yet is stale then (a pad found low before the world, plugged in on the character screen).
    [[nodiscard]] static bool IsFine(int percent, bool onBattery);

    // A pad gone: the next one starts afresh.
    void Reset();

private:
    // 0 nothing said, 1 the 20 % line said, 2 the 10 % line said too.
    int m_Said = 0;
};
} // namespace Core::Input::Gamepad
