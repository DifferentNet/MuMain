// MU Helper's crafting materials option (ghinzuro, 2026-11-20): Loch's Feather / Crest of Monarch, Condor's
// feather and flame, the Dark Lord pet Spirit and the Fenrir materials are picked up with their own option.
// The quest option leaves them out, while the loot filter still files the feathers and the Spirit as Quest.

#include "stdafx.h"

#include "doctest.h"

#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Items/ItemCategories.h"

using GameLogic::Items::IsCraftingMaterialItem;
using GameLogic::Items::IsQuestItem;

namespace
{
ITEM MakeItem(int itemType, int level = 0)
{
    ITEM item{};
    item.Type = static_cast<short>(itemType);
    item.Level = level;
    return item;
}

// What the helper's quest option picks up (MuHelper.cpp, IsHelperQuestItem).
bool IsHelperQuestItem(const ITEM& item)
{
    return IsQuestItem(&item) && !IsCraftingMaterialItem(&item);
}
} // namespace

TEST_CASE("Feathers, the Spirit and the Fenrir materials are crafting materials, not helper quest items")
{
    constexpr int kLevels[] = {0, 1};
    for (const int level : kLevels)
    {
        const ITEM feather = MakeItem(ITEM_LOCHS_FEATHER, level); // level 1: Crest of Monarch
        const ITEM spirit = MakeItem(ITEM_SPIRIT, level);         // level 1: Spirit of Dark Raven
        CHECK(IsCraftingMaterialItem(&feather));
        CHECK(IsCraftingMaterialItem(&spirit));
        CHECK_FALSE(IsHelperQuestItem(feather));
        CHECK_FALSE(IsHelperQuestItem(spirit));
    }

    constexpr int kMaterials[] = {ITEM_FEATHER_OF_CONDOR, ITEM_FLAME_OF_CONDOR, ITEM_SPLINTER_OF_ARMOR,
                                  ITEM_BLESS_OF_GUARDIAN, ITEM_CLAW_OF_BEAST, ITEM_FRAGMENT_OF_HORN, ITEM_BROKEN_HORN};
    for (const int itemType : kMaterials)
    {
        const ITEM item = MakeItem(itemType);
        CHECK(IsCraftingMaterialItem(&item));
        CHECK_FALSE(IsHelperQuestItem(item));
    }
}

TEST_CASE("The loot filter still files the feathers and the Spirit as Quest")
{
    const ITEM feather = MakeItem(ITEM_LOCHS_FEATHER);
    const ITEM condorFeather = MakeItem(ITEM_FEATHER_OF_CONDOR);
    const ITEM spirit = MakeItem(ITEM_SPIRIT, 1);
    CHECK(IsQuestItem(&feather));
    CHECK(IsQuestItem(&condorFeather));
    CHECK(IsQuestItem(&spirit));
}

TEST_CASE("Event tickets stay quest items, not crafting materials")
{
    const ITEM bloodCastleTicket = MakeItem(ITEM_INVISIBILITY_CLOAK);
    const ITEM symbolOfKundun = MakeItem(ITEM_SYMBOL_OF_KUNDUN);
    CHECK(IsHelperQuestItem(bloodCastleTicket));
    CHECK(IsHelperQuestItem(symbolOfKundun));
    CHECK_FALSE(IsCraftingMaterialItem(&bloodCastleTicket));
    CHECK_FALSE(IsCraftingMaterialItem(&symbolOfKundun));
}
