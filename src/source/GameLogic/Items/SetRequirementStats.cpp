#include "GameLogic/Items/SetRequirementStats.h"

#include "GameLogic/Items/AncientAura.h"

#include <algorithm>

namespace GameLogic::SetRequirementStats
{
namespace
{
// The clamp of AddGrandStat / AddAncientTierStat (Engine/Object/ZzzInfomation.cpp): a total already above MaxStat
// stays as it is.
int AddCapped(int total, int bonus)
{
    const int room = std::max(0, MaxStat - total);
    return total + std::min(std::max(bonus, 0), room);
}

int AddTierShare(int total, int tier)
{
    return AddCapped(total, AncientAura::StatBonus(total, tier));
}
} // namespace

Totals WithGrandPoints(Totals totals, const Items::GrandStatsLine::Points& points)
{
    totals.strength = AddCapped(totals.strength, points.strength);
    totals.agility = AddCapped(totals.agility, points.agility);
    totals.vitality = AddCapped(totals.vitality, points.vitality);
    totals.energy = AddCapped(totals.energy, points.energy);
    totals.command = AddCapped(totals.command, points.command);
    return totals;
}

Totals WithAncientTier(Totals totals, int tier)
{
    if (tier == AncientAura::None)
        return totals;

    totals.strength = AddTierShare(totals.strength, tier);
    totals.agility = AddTierShare(totals.agility, tier);
    totals.vitality = AddTierShare(totals.vitality, tier);
    totals.energy = AddTierShare(totals.energy, tier);
    totals.command = AddTierShare(totals.command, tier);
    return totals;
}

Totals WithGrandPointsAndTier(Totals totals, const Items::GrandStatsLine::Points& points, int tier)
{
    return WithAncientTier(WithGrandPoints(totals, points), tier);
}
} // namespace GameLogic::SetRequirementStats
