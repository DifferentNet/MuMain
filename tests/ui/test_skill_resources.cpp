#include <doctest.h>

#include "GameLogic/Skills/SkillResources.h"

using GameLogic::Skills::HasResourcesFor;

TEST_CASE("A skill is castable with exactly the mana and AG it spends")
{
    CHECK(HasResourcesFor(10, 5, 10, 5));
    CHECK(HasResourcesFor(100, 50, 10, 5));
}

TEST_CASE("Short mana or short AG greys the skill")
{
    CHECK_FALSE(HasResourcesFor(9, 50, 10, 5));
    CHECK_FALSE(HasResourcesFor(100, 4, 10, 5));
    CHECK_FALSE(HasResourcesFor(0, 0, 1, 1));
}

TEST_CASE("Free skills never need resources")
{
    CHECK(HasResourcesFor(0, 0, 0, 0));
    CHECK(HasResourcesFor(0, 0, -3, -1));
}
