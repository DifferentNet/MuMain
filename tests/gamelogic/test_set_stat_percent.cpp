#include "doctest.h"

#include "GameLogic/Items/SetRequirementStats.h"
#include "GameLogic/Items/SetStatPercent.h"

namespace Percent = GameLogic::SetStatPercent;
using GameLogic::SetRequirementStats::Totals;
using GameLogic::SetRequirementStats::WithSetPercents;

TEST_CASE("A stat percentage option raises its own stat only")
{
    Percent::Factors factors;
    CHECK(Percent::AddOption(factors, Percent::StrengthPercent, 5));
    CHECK(factors.strength == doctest::Approx(1.05));
    CHECK(factors.agility == 1.0);
    CHECK(factors.energy == 1.0);
}

TEST_CASE("Two options multiply, as the server's Multiplicate elements do")
{
    Percent::Factors factors;
    Percent::AddOption(factors, Percent::AgilityPercent, 4);
    Percent::AddOption(factors, Percent::AgilityPercent, 4);
    CHECK(factors.agility == doctest::Approx(1.0816));
    CHECK(Percent::Apply(10000, factors.agility) == 10816);
}

TEST_CASE("Mana, AG and the other options don't change the five stats")
{
    Percent::Factors factors;
    CHECK_FALSE(Percent::AddOption(factors, Percent::ManaPercent, 10));
    CHECK_FALSE(Percent::AddOption(factors, Percent::AgPercent, 5));
    CHECK_FALSE(Percent::AddOption(factors, 0, 125)); // flat strength
    CHECK(factors == Percent::Factors{});
}

TEST_CASE("Apply rounds down, keeps exact products and stays within a WORD")
{
    CHECK(Percent::Apply(20, 1.05) == 21);
    CHECK(Percent::Apply(1001, 1.05) == 1051);
    CHECK(Percent::Apply(0, 1.05) == 0);
    CHECK(Percent::Apply(1000, 1.0) == 1000);
    CHECK(Percent::Apply(64000, 1.05) == Percent::MaxStat);
}

TEST_CASE("The requirement totals take the set percentages and leave the level alone")
{
    Percent::Factors factors;
    Percent::AddOption(factors, Percent::StrengthPercent, 5);
    Percent::AddOption(factors, Percent::VitalityPercent, 3);
    const Totals raised = WithSetPercents(Totals{ 2000, 1000, 1000, 3000, 0, 400 }, factors);
    CHECK(raised == Totals{ 2100, 1000, 1030, 3000, 0, 400 });
}
