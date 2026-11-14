#pragma once

#include <algorithm>

// The "Skill Damage: N %" line of the character window (UI/Character/CharacterInfoWindow): how much a class's skills
// multiply its damage, as our server counts it (MU Skynet balance audit, ghinzuro 2026-11-14). The original S6 client
// showed 200 + energy / 10 for the Dark Knight, 200 for the Magic Gladiator and 200 + energy / 20 for the Dark Lord,
// without any cap; the server never worked like that.
//   Dark Knight:      2 + min(energy, 7000) / 1000   -> 200 % .. 900 % (the cap at 7,000 energy)
//   Magic Gladiator:  1.5                            -> 150 %
//   Dark Lord:        1 + min(energy, 12000) / 4000  -> 100 % .. 400 % (the cap at 12,000 energy)
// The energy is the total the window shows (base + bonus, Grand stat points included).
namespace GameLogic::Skills::SkillDamagePercent
{
inline constexpr int KnightBasePercent = 200;
inline constexpr int KnightEnergyCap = 7000;
inline constexpr int KnightEnergyPerPercent = 10;
inline constexpr int KnightMaxPercent = KnightBasePercent + KnightEnergyCap / KnightEnergyPerPercent;

inline constexpr int MagicGladiatorPercent = 150;

inline constexpr int DarkLordBasePercent = 100;
inline constexpr int DarkLordEnergyCap = 12000;
inline constexpr int DarkLordEnergyPerPercent = 40;
inline constexpr int DarkLordMaxPercent = DarkLordBasePercent + DarkLordEnergyCap / DarkLordEnergyPerPercent;

[[nodiscard]] constexpr int Knight(int energy)
{
    const int countedEnergy = std::clamp(energy, 0, KnightEnergyCap);
    return KnightBasePercent + countedEnergy / KnightEnergyPerPercent;
}

[[nodiscard]] constexpr int MagicGladiator()
{
    return MagicGladiatorPercent;
}

[[nodiscard]] constexpr int DarkLord(int energy)
{
    const int countedEnergy = std::clamp(energy, 0, DarkLordEnergyCap);
    return DarkLordBasePercent + countedEnergy / DarkLordEnergyPerPercent;
}
} // namespace GameLogic::Skills::SkillDamagePercent
