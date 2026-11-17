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
    // Level 4 and 3: all three; 2: pools and soft, no flipbook; 0 and 1: none.
    for (int level : {3, 4})
    {
        CHECK(SkillFx::LevelAllows(Feature::LightPools, level));
        CHECK(SkillFx::LevelAllows(Feature::SoftParticles, level));
        CHECK(SkillFx::LevelAllows(Feature::FlipbookBlend, level));
    }
    CHECK(SkillFx::LevelAllows(Feature::LightPools, 2));
    CHECK(SkillFx::LevelAllows(Feature::SoftParticles, 2));
    CHECK_FALSE(SkillFx::LevelAllows(Feature::FlipbookBlend, 2));
    for (int level : {0, 1})
    {
        CHECK_FALSE(SkillFx::LevelAllows(Feature::LightPools, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::SoftParticles, level));
        CHECK_FALSE(SkillFx::LevelAllows(Feature::FlipbookBlend, level));
    }
}

TEST_CASE("a look shows only when chosen, offered and allowed by the level [render][skill_fx]")
{
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
}
