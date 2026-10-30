#include "UI/Character/StatPointsMenu.h"

#include <array>

namespace UI::Character::StatPoints
{
namespace
{
constexpr std::array<int, 3> Steps = {10, 100, 1000};
constexpr std::array<const wchar_t*, 5> Commands = {L"/addstr", L"/addagi", L"/addvit", L"/addene", L"/addcmd"};
} // namespace

std::vector<int> MenuAmounts(int freePoints)
{
    std::vector<int> amounts;
    if (freePoints <= 0)
        return amounts;
    for (const int step : Steps)
    {
        if (step < freePoints)
            amounts.push_back(step);
    }
    amounts.push_back(freePoints);
    return amounts;
}

std::wstring Command(int stat, int amount)
{
    if (stat < 0 || stat >= static_cast<int>(Commands.size()) || amount <= 0)
        return {};
    return std::wstring(Commands[static_cast<size_t>(stat)]) + L" " + std::to_wstring(amount);
}
} // namespace UI::Character::StatPoints
