#pragma once

// The ancient set options that raise a total stat by a percentage (MU Skynet, ghinzuro 2026-11-26: the flat +70..125
// strength, agility, energy, vitality and command of the sets were 0.2-1 % of a character's points). The server's
// scripts/ancient-options.py writes them as option codes 41-47 of itemsetoption_*.bmd; on the server each is a
// Multiplicate element of the total stat (the base and every bonus), so two +4 % make x1.04 x 1.04. Mana and AG
// (46, 47) are the server's to apply: the client only names them.
namespace GameLogic::SetStatPercent
{
inline constexpr int StrengthPercent = 41;
inline constexpr int AgilityPercent = 42;
inline constexpr int EnergyPercent = 43;
inline constexpr int VitalityPercent = 44;
inline constexpr int CommandPercent = 45;
inline constexpr int ManaPercent = 46;
inline constexpr int AgPercent = 47;

// The client keeps each stat in a WORD: a bonus never raises a total above this.
inline constexpr int MaxStat = 65535;

struct Factors
{
    double strength = 1.0;
    double agility = 1.0;
    double vitality = 1.0;
    double energy = 1.0;
    double command = 1.0;

    bool operator==(const Factors&) const = default;
};

// Multiplies in the option when it raises one of the five stats; true when it did.
constexpr bool AddOption(Factors& factors, int optionCode, int percent)
{
    double* factor = nullptr;
    switch (optionCode)
    {
    case StrengthPercent: factor = &factors.strength; break;
    case AgilityPercent: factor = &factors.agility; break;
    case EnergyPercent: factor = &factors.energy; break;
    case VitalityPercent: factor = &factors.vitality; break;
    case CommandPercent: factor = &factors.command; break;
    default: return false;
    }

    *factor *= 1.0 + percent / 100.0;
    return true;
}

// The total with the factor applied, rounded down as the server reads its total (a tiny margin keeps 20 x 1.05
// at 21), never below the total and never above MaxStat.
constexpr int Apply(int total, double factor)
{
    if (total <= 0 || factor <= 1.0)
    {
        return total;
    }

    const double raised = static_cast<double>(total) * factor + 1e-6;
    if (raised >= MaxStat)
    {
        return total > MaxStat ? total : MaxStat;
    }

    return static_cast<int>(raised);
}
} // namespace GameLogic::SetStatPercent
