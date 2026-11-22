#pragma once

#include "GameLogic/Items/GrandStatsLine.h"

// The stat totals the server checks a worn item's requirements against (MU Skynet): the base and every item and set
// bonus, then the Grand stat points (GameLogic/Items/GrandStats.h), then the +14/+15 ancient tier
// (GameLogic/Items/AncientAura.h), which multiplies the Grand points too. CHARACTER_MACHINE::CalculateAll adds the
// Grand points and the tier only after the set options are worked out, so CSItemOption::CheckItemSetOptions adds
// them here; without them a set worn thanks to Grand points showed inactive while the server applied it.
namespace GameLogic::SetRequirementStats
{
// The client keeps each stat in a WORD: a bonus never raises a total above this (as CalculateAll does).
inline constexpr int MaxStat = 65535;

struct Totals
{
    int strength = 0;
    int agility = 0;
    int vitality = 0;
    int energy = 0;
    int command = 0;
    int level = 0;

    bool operator==(const Totals&) const = default;
};

// The totals with the Grand stat points added; negative points count as 0.
[[nodiscard]] Totals WithGrandPoints(Totals totals, const Items::GrandStatsLine::Points& points);

// The totals with the ancient tier's share of each stat added (AncientAura::StatBonus); AncientAura::None adds
// nothing.
[[nodiscard]] Totals WithAncientTier(Totals totals, int tier);

// The Grand points first, then the tier, in the server's order.
[[nodiscard]] Totals WithGrandPointsAndTier(Totals totals, const Items::GrandStatsLine::Points& points, int tier);
} // namespace GameLogic::SetRequirementStats
