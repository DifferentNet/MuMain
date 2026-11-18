#include <doctest.h>

#include "Render/Effects/SkillFx.h"

#include <string>

namespace
{
namespace SkillFx = Render::Effects::SkillFx;
using SkillFx::Feature;
} // namespace

TEST_CASE("the effect level sets the pool budget [render][skill_fx]")
{
    CHECK(SkillFx::PoolBudget(4) == 16);
    CHECK(SkillFx::PoolBudget(5) == SkillFx::kMaxPools);
    CHECK(SkillFx::PoolBudget(3) == 12);
    CHECK(SkillFx::PoolBudget(2) == 8);
    CHECK(SkillFx::PoolBudget(1) == 0);
    CHECK(SkillFx::PoolBudget(0) == 0);
    CHECK(SkillFx::PoolBudget(-1) == 0);
}

TEST_CASE("the effect level lets each look show [render][skill_fx]")
{
    // Level 4 and 3: all six; 2: pools, soft, decals and cores, no flipbook or trails; 0 and 1: none.
    for (int level : {3, 4})
    {
        CHECK(SkillFx::LevelAllows(Feature::LightPools, level));
        CHECK(SkillFx::LevelAllows(Feature::SoftParticles, level));
        CHECK(SkillFx::LevelAllows(Feature::FlipbookBlend, level));
        CHECK(SkillFx::LevelAllows(Feature::Ribbons, level));
        CHECK(SkillFx::LevelAllows(Feature::Decals, level));
        CHECK(SkillFx::LevelAllows(Feature::Cores, level));
    }
    CHECK(SkillFx::LevelAllows(Feature::LightPools, 2));
    CHECK(SkillFx::LevelAllows(Feature::SoftParticles, 2));
    CHECK_FALSE(SkillFx::LevelAllows(Feature::FlipbookBlend, 2));
    CHECK_FALSE(SkillFx::LevelAllows(Feature::Ribbons, 2));
    CHECK(SkillFx::LevelAllows(Feature::Decals, 2));
    CHECK(SkillFx::LevelAllows(Feature::Cores, 2));
    for (int level : {0, 1})
    {
        CHECK_FALSE(SkillFx::LevelAllows(Feature::LightPools, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::SoftParticles, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::FlipbookBlend, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::Ribbons, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::Decals, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::Cores, level));
    }
}

TEST_CASE("the effect level sets the smooth trail budgets [render][skill_fx]")
{
    CHECK(SkillFx::RibbonPieceBudget(5) == 8);
    CHECK(SkillFx::RibbonPieceBudget(4) == 8);
    CHECK(SkillFx::RibbonPieceBudget(3) == 4);
    for (int level : {2, 1, 0, -1})
    {
        CHECK(SkillFx::RibbonPieceBudget(level) == 0);
        CHECK(SkillFx::RibbonQuadBudget(level) == 0);
    }
    CHECK(SkillFx::RibbonQuadBudget(4) == 4096);
    CHECK(SkillFx::RibbonQuadBudget(3) == 2048);
}

TEST_CASE("the effect level sets the ground mark budget [render][skill_fx]")
{
    CHECK(SkillFx::MarkBudget(5) == SkillFx::kMaxMarks);
    CHECK(SkillFx::MarkBudget(4) == 48);
    CHECK(SkillFx::MarkBudget(3) == 32);
    CHECK(SkillFx::MarkBudget(2) == 16);
    for (int level : {1, 0, -1})
    {
        CHECK(SkillFx::MarkBudget(level) == 0);
    }
}

TEST_CASE("the effect level sets the hot core strength [render][skill_fx]")
{
    CHECK(SkillFx::CoreStrength(5) == doctest::Approx(1.0f));
    CHECK(SkillFx::CoreStrength(4) == doctest::Approx(1.0f));
    CHECK(SkillFx::CoreStrength(3) == doctest::Approx(0.75f));
    CHECK(SkillFx::CoreStrength(2) == doctest::Approx(0.5f));
    for (int level : {1, 0, -1})
    {
        CHECK(SkillFx::CoreStrength(level) == 0.0f);
    }
}

TEST_CASE("a look shows only when chosen, offered and allowed by the level [render][skill_fx]")
{
    CHECK(SkillFx::kFeatureCount == 6);
    for (int index = 0; index < SkillFx::kFeatureCount; ++index)
    {
        const auto feature = static_cast<Feature>(index);
        CHECK(SkillFx::Resolve(true, true, feature, 4));
        // Not chosen ($fx ... off, the ini key 0).
        CHECK_FALSE(SkillFx::Resolve(false, true, feature, 4));
        // Not offered: the Classic client and the web.
        CHECK_FALSE(SkillFx::Resolve(true, false, feature, 4));
        // Effect level 1.
        CHECK_FALSE(SkillFx::Resolve(true, true, feature, 1));
    }
}

TEST_CASE("the console names the looks [render][skill_fx]")
{
    CHECK(std::string(SkillFx::Name(Feature::LightPools)) == "pools");
    CHECK(std::string(SkillFx::Name(Feature::SoftParticles)) == "soft");
    CHECK(std::string(SkillFx::Name(Feature::FlipbookBlend)) == "flipbook");
    CHECK(std::string(SkillFx::Name(Feature::Ribbons)) == "trails");
    CHECK(std::string(SkillFx::Name(Feature::Decals)) == "decals");
    CHECK(std::string(SkillFx::Name(Feature::Cores)) == "cores");
}
