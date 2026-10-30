// The "+" buttons of the character window add one point a click, and a reset gives 500. A right
// click on a "+" (a finger's long press, R3 on a pad) opens a menu of bigger steps instead, spent
// through the server's /addstr and the like (OpenMU's IncreaseStatsAction behind them stops at a
// stat's maximum and keeps the points it could not add).
#pragma once

#include <string>
#include <vector>

namespace UI::Character::StatPoints
{
// The steps the menu offers for that many free points: 10, 100 and 1000 while fewer than all of
// them, then all of them. Nothing without free points.
[[nodiscard]] std::vector<int> MenuAmounts(int freePoints);

// The chat command that spends `amount` points on the stat of that "+" (0 strength, 1 agility,
// 2 vitality, 3 energy, 4 command); empty for another stat or no points.
[[nodiscard]] std::wstring Command(int stat, int amount);
} // namespace UI::Character::StatPoints
