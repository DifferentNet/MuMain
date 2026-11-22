#include "doctest.h"

#include "GameLogic/Items/AncientAura.h"
#include "GameLogic/Items/GrandStatsLine.h"
#include "GameLogic/Items/SetRequirementStats.h"

using GameLogic::Items::GrandStatsLine::Points;
using GameLogic::SetRequirementStats::MaxStat;
using GameLogic::SetRequirementStats::Totals;
using GameLogic::SetRequirementStats::WithAncientTier;
using GameLogic::SetRequirementStats::WithGrandPoints;
using GameLogic::SetRequirementStats::WithGrandPointsAndTier;

namespace
{
// A Brass armour +13 piece (~346 strength) on a Magic Gladiator with base strength 26 (the "slayer" report).
constexpr int BrassStrengthRequirement = 346;
constexpr int BaseStrength = 26;

bool MeetsStrength(const Totals& totals)
{
    return BrassStrengthRequirement <= totals.strength;
}
} // namespace

TEST_CASE("Grand points count towards an item's strength requirement")
{
    const Totals base{ BaseStrength, 0, 0, 0, 0, 400 };

    CHECK_FALSE(MeetsStrength(WithGrandPoints(base, Points{})));
    CHECK(MeetsStrength(WithGrandPoints(base, Points{ 320, 0, 0, 0, 0 })));
    CHECK_FALSE(MeetsStrength(WithGrandPoints(base, Points{ 319, 0, 0, 0, 0 })));
    CHECK(WithGrandPoints(base, Points{ 1000, 0, 0, 0, 0 }).strength == 1026);
}

TEST_CASE("Grand points go to the stat they were put into and leave the level alone")
{
    const Totals base{ 10, 20, 30, 40, 50, 400 };
    const Totals withPoints = WithGrandPoints(base, Points{ 1, 2, 3, 4, 5 });

    CHECK(withPoints == Totals{ 11, 22, 33, 44, 55, 400 });
}

TEST_CASE("Grand points never raise a stat above the WORD the client keeps it in")
{
    CHECK(WithGrandPoints(Totals{ 65000 }, Points{ 1000 }).strength == MaxStat);
    CHECK(WithGrandPoints(Totals{ MaxStat }, Points{ 1 }).strength == MaxStat);
    CHECK(WithGrandPoints(Totals{ 1 }, Points{ 2147483647 }).strength == MaxStat);
    // A total already above the WORD (base plus bonuses summed as int) stays as it is, as in CalculateAll.
    CHECK(WithGrandPoints(Totals{ 70000 }, Points{ 10 }).strength == 70000);
}

TEST_CASE("Negative Grand points count as none")
{
    const Totals base{ 100, 100, 100, 100, 100, 1 };

    CHECK(WithGrandPoints(base, Points{ -50, -1, -2147483647, 0, -10 }) == base);
}

TEST_CASE("The ancient tier adds its share of each stat")
{
    const Totals base{ 1000, 2000, 300, 40, 9, 400 };

    CHECK(WithAncientTier(base, GameLogic::AncientAura::None) == base);
    CHECK(WithAncientTier(base, GameLogic::AncientAura::Plus14) == Totals{ 1050, 2100, 315, 42, 9, 400 });
    CHECK(WithAncientTier(base, GameLogic::AncientAura::Plus15) == Totals{ 1100, 2200, 330, 44, 9, 400 });
    CHECK(WithAncientTier(Totals{ 65000 }, GameLogic::AncientAura::Plus15).strength == MaxStat);
}

TEST_CASE("The ancient tier multiplies the Grand points too")
{
    const Totals base{ 1000 };
    const Points points{ 1000 };

    // (1000 + 1000) * 1.10, not 1000 * 1.10 + 1000.
    CHECK(WithGrandPointsAndTier(base, points, GameLogic::AncientAura::Plus15).strength == 2200);
    CHECK(WithGrandPointsAndTier(base, points, GameLogic::AncientAura::Plus14).strength == 2100);
    CHECK(WithGrandPointsAndTier(base, points, GameLogic::AncientAura::None).strength == 2000);
}
