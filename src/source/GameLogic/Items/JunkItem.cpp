#include "stdafx.h"

#include "GameLogic/Items/JunkItem.h"

#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/ShopRestrictions.h"

namespace GameLogic::Items::JunkMarks
{
namespace
{
Kind KindOf(const ITEM& item)
{
    if (item.Type < 0 || item.Type >= MAX_ITEM)
        return Kind::Other;
    const int group = item.Type / MAX_ITEM_INDEX;
    if (group >= ITEM_GROUP_SWORD && group <= ITEM_GROUP_STAFF)
        return GameLogic::Items::IsAmmunition(&item) ? Kind::Other : Kind::Weapon;
    if (group == ITEM_GROUP_SHIELD)
        return Kind::Shield;
    if (group >= ITEM_GROUP_HELM && group <= ITEM_GROUP_BOOTS)
        return Kind::Armour;
    return Kind::Other;
}

// The chaos weapons go into the wing mixes (CMixRecipes::IsChaosItem): kept, plain as they look.
bool IsMixIngredient(const ITEM& item)
{
    return item.Type == ITEM_CHAOS_DRAGON_AXE || item.Type == ITEM_CHAOS_NATURE_BOW ||
           item.Type == ITEM_CHAOS_LIGHTNING_STAFF;
}

bool IsSellable(const ITEM& item)
{
    // Rented gear and the lucky sets are never junk either.
    if (item.bPeriodItem || GameLogic::Items::IsLuckyItemType(item.Type))
        return false;
    return !GameLogic::Items::IsSellingBan(&item);
}
} // namespace

Traits ReadTraits(const ITEM& item)
{
    Traits traits;
    traits.kind = KindOf(item);
    if (traits.kind == Kind::Other)
        return traits;
    traits.level = item.Level;
    traits.excellent = item.ExcellentFlags != 0;
    traits.ancient = item.AncientDiscriminator != 0;
    traits.luck = item.HasLuck;
    traits.sockets = item.SocketCount > 0;
    traits.harmony = item.Jewel_Of_Harmony_Option != 0;
    traits.sellable = IsSellable(item);
    traits.highValue = GameLogic::Items::IsHighValueItem(&item);
    traits.questOrEvent = GameLogic::Items::IsQuestItem(&item);
    traits.mixIngredient = IsMixIngredient(item);
    return traits;
}

Fingerprint FingerprintOf(const ITEM& item)
{
    Identity identity;
    identity.type = item.Type;
    identity.level = item.Level;
    identity.optionLevel = item.OptionLevel;
    identity.excellentFlags = item.ExcellentFlags;
    identity.ancient = item.AncientDiscriminator;
    identity.luck = item.HasLuck;
    identity.skill = item.HasSkill;
    identity.harmonyOption = item.Jewel_Of_Harmony_Option;
    identity.socketCount = item.SocketCount;
    return MakeFingerprint(identity);
}

bool MayBeJunk(const ITEM& item)
{
    return CanBeJunk(ReadTraits(item));
}
} // namespace GameLogic::Items::JunkMarks
