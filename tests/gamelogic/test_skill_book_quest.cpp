// Which skill books ask for Marlon's "Gain Hero Status" before they are read: the server's (OpenMU's item
// requirements), or the right click refuses books the server would teach.

#include "stdafx.h"

#include "doctest.h"

#include "GameLogic/Items/SkillBookQuest.h"

using GameLogic::Items::SkillBookNeedsHeroStatusQuest;

TEST_CASE("The Crystal of Destruction asks for the hero status quest")
{
    CHECK(SkillBookNeedsHeroStatusQuest(ITEM_CRYSTAL_OF_DESTRUCTION));
}

TEST_CASE("Nova, Wizardry Enhance, Multi-Shot and Recovery ask for no quest")
{
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_SCROLL_OF_NOVA));
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_SCROLL_OF_WIZARDRY_ENHANCE));
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_CRYSTAL_OF_MULTI_SHOT));
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_CRYSTAL_OF_RECOVERY));
}

TEST_CASE("The other skill books ask for no quest")
{
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_SCROLL_OF_CHAOTIC_DISEIER));
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_CRYSTAL_OF_FLAME_STRIKE));
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_SCROLL_OF_GIGANTIC_STORM));
    CHECK_FALSE(SkillBookNeedsHeroStatusQuest(ITEM_SCROLL_OF_POISON));
}
