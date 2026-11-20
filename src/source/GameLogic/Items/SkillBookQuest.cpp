#include "stdafx.h"
#include "GameLogic/Items/SkillBookQuest.h"

namespace GameLogic::Items
{
bool SkillBookNeedsHeroStatusQuest(int itemType)
{
    return itemType == ITEM_CRYSTAL_OF_DESTRUCTION;
}
} // namespace GameLogic::Items
