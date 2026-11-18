#include "UI/Combat/MonsterHealthBar.h"

#include <cmath>
#include <cwchar>
#include <iterator>

namespace
{
// The server sends a monster's health in 1/250 steps (Network/Server/WSclient.cpp, ReceiveMonsterHealth).
constexpr double HealthSteps = 250.0;
// Below this percent the text shows one decimal: a world boss at 4.8 % is not "5%".
constexpr double DecimalBelowPercent = 10.0;
} // namespace

bool UI::Combat::HealthBar::ShouldRenderSelected(float healthStatus)
{
    return healthStatus != 0.0f;
}

std::wstring UI::Combat::HealthBar::PercentText(float healthStatus)
{
    // Not known yet (below 0) or dead (0, the bar is hidden too): the name alone.
    if (healthStatus <= 0.0f)
        return {};
    // Same rounding as the control bridge's hp_percent (App/Control/ControlState.cpp).
    const double percent = std::round(healthStatus * HealthSteps) * 100.0 / HealthSteps;
    wchar_t text[16]{};
    if (percent < DecimalBelowPercent)
        std::swprintf(text, std::size(text), L"%.1f%%", percent);
    else
        std::swprintf(text, std::size(text), L"%.0f%%", percent);
    return text;
}
