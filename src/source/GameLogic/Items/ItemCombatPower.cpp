#include "stdafx.h"
#include "GameLogic/Items/ItemCombatPower.h"

#include "Character/CharacterManager.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Data/GameData/ItemData/ItemDefinition.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Items/ItemCategories.h"

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cwchar>
#include <iterator>
#include <utility>
#include <vector>

namespace
{
using Data::Items::ItemSlot;
using GameLogic::Items::CombatPowerEquipment;
using GameLogic::Items::CombatPowerItem;

// The numbers below are the server's (CombatPower.cs); keep them equal, or the tooltip and /cp disagree.
constexpr std::array<int, 16> LevelPoints = {0, 10, 20, 30, 45, 60, 80, 100, 170, 250, 370, 500, 690, 900, 1200, 1500};
constexpr std::array<int, 7> ExcellentPoints = {0, 150, 350, 600, 900, 1200, 1500};
constexpr int OptionStepPoints = 20;
constexpr int LuckPoints = 50;
constexpr int DropLevelFactor = 10;
constexpr int JewelleryDropLevelFactor = 5;
constexpr std::uint8_t ExcellentOptionMask = 0x3F;

constexpr int AncientPiecePoints = 250;
constexpr int HighTierAncientPiecePoints = 500;
constexpr int PowerOfTheAncientsPoints = 1500;

constexpr int ItemGroupSword = 0;
constexpr int ItemGroupBow = 4;
constexpr int ItemGroupStaff = 5;
constexpr int ItemGroupWings = 12;
constexpr int ItemGroupMisc = 13;
constexpr int BoltNumber = 7;
constexpr int ArrowsNumber = 15;
constexpr int DarkRavenNumber = 5;

// Fenrir's kind is in the excellent flags, as the item tooltip reads it.
constexpr int FenrirNumber = 37;
constexpr std::uint8_t BlackFenrirFlag = 0x01;
constexpr std::uint8_t BlueFenrirFlag = 0x02;
constexpr std::uint8_t GoldFenrirFlag = 0x04;

// Equipment indexes (EQUIPMENT_*) that are not item slots of their own.
constexpr int RightHandIndex = 0;
constexpr int LeftHandIndex = 1;
constexpr int SecondRingIndex = 11;

// The high-tier ancient sets (the server's HighTierAncients.SetNames).
constexpr std::array<const wchar_t*, 10> HighTierSetNames = {
    L"Mahes", L"Bragi", L"Serket", L"Frigg", L"Bes", L"Alvis", L"Apis", L"Tyr", L"Khone", L"Surt"};

// Items whose level in the client's item table differs from the server's; the server's counts.
struct DropLevelOverride
{
    int Group;
    int Number;
    int DropLevel;
};

constexpr std::array<DropLevelOverride, 6> DropLevelOverrides = {{
    {7, 44, 110}, {8, 44, 113}, {9, 44, 102}, {10, 44, 82}, {11, 44, 90}, // Lilium set (the client's "Eternal Wing")
    {13, 76, 28},                                                          // Panda Transformation Ring
}};

int GetLevelPoints(int level)
{
    return LevelPoints[std::clamp(level, 0, static_cast<int>(LevelPoints.size()) - 1)];
}

int GetDropLevel(const CombatPowerItem& item)
{
    for (const auto& entry : DropLevelOverrides)
    {
        if (entry.Group == item.Group && entry.Number == item.Number)
        {
            return entry.DropLevel;
        }
    }

    return item.DropLevel;
}

int GetWingPoints(const CombatPowerItem& item)
{
    const int number = item.Number;
    int basePoints = 0;
    if (item.Group == ItemGroupWings && ((number >= 0 && number <= 2) || number == 41))
    {
        basePoints = 500; // 1st wings
    }
    else if ((item.Group == ItemGroupWings && ((number >= 3 && number <= 6) || number == 42))
             || (item.Group == ItemGroupMisc && number == 30))
    {
        basePoints = 1500; // 2nd wings and the Cape of Lord
    }
    else if (item.Group == ItemGroupWings && ((number >= 36 && number <= 40) || number == 43))
    {
        basePoints = 4000; // 3rd wings and the Cape of Emperor
    }

    return basePoints == 0 ? 0 : basePoints + 2 * GetLevelPoints(item.Level);
}

int GetPetPoints(const CombatPowerItem& item)
{
    if (item.Group != ItemGroupMisc)
    {
        return 0;
    }

    switch (item.Number)
    {
    case FenrirNumber:
        if ((item.ExcellentFlags & (BlackFenrirFlag | GoldFenrirFlag)) != 0)
        {
            return 1000;
        }

        return (item.ExcellentFlags & BlueFenrirFlag) != 0 ? 800 : 500;
    case 3: return 200; // Horn of Dinorant
    case 4: return 700; // Dark Horse
    default: return 0;
    }
}

int GetGearPoints(const CombatPowerItem& item)
{
    const bool isJewellery = item.Slot == static_cast<std::uint8_t>(ItemSlot::Pendant)
                             || item.Slot == static_cast<std::uint8_t>(ItemSlot::Ring);
    int points = (isJewellery ? JewelleryDropLevelFactor : DropLevelFactor) * GetDropLevel(item);
    points += GetLevelPoints(item.Level);
    const int excellentCount = std::popcount(static_cast<unsigned>(item.ExcellentFlags & ExcellentOptionMask));
    points += ExcellentPoints[std::min(excellentCount, static_cast<int>(ExcellentPoints.size()) - 1)];
    points += OptionStepPoints * item.OptionLevel;
    if (item.HasLuck)
    {
        points += LuckPoints;
    }

    return points;
}

bool IsGear(const CombatPowerItem& item)
{
    return item.Slot != static_cast<std::uint8_t>(ItemSlot::Wings) && item.Slot != static_cast<std::uint8_t>(ItemSlot::Pet)
           && item.Slot != static_cast<std::uint8_t>(ItemSlot::None)
           && !(item.Group == ItemGroupMisc && item.Number == DarkRavenNumber);
}

// The item's points without its share of an ancient set: the server's items, wings or pets part.
int GetOwnPoints(const CombatPowerItem& item)
{
    if (item.Slot == static_cast<std::uint8_t>(ItemSlot::Wings))
    {
        return GetWingPoints(item);
    }

    if (item.Slot == static_cast<std::uint8_t>(ItemSlot::Pet))
    {
        return GetPetPoints(item);
    }

    return IsGear(item) ? GetGearPoints(item) : 0;
}

int GetAncientPiecePoints(const CombatPowerItem& item)
{
    return item.IsHighTierAncient ? HighTierAncientPiecePoints : AncientPiecePoints;
}

bool IsAmmunition(const CombatPowerItem& item)
{
    return item.Group == ItemGroupBow && (item.Number == BoltNumber || item.Number == ArrowsNumber);
}

bool IsBowOrCrossbow(const CombatPowerItem& item)
{
    return item.Group == ItemGroupBow && !IsAmmunition(item);
}

// A bow or crossbow and its ammunition, which are held together.
bool AreHeldTogether(const CombatPowerItem& first, const CombatPowerItem& second)
{
    return (IsBowOrCrossbow(first) && IsAmmunition(second)) || (IsAmmunition(first) && IsBowOrCrossbow(second));
}

// The server's PowerOfTheAncientsPlugIn: a full ancient set gives the bonus only with the class's weapon.
bool HasPowerOfTheAncientsWeapon(const CombatPowerEquipment& equipment, int baseClass)
{
    bool twoHandedSword = false;
    bool twoHandedStaff = false;
    bool bow = false;
    bool raven = false;
    for (const CombatPowerItem& item : equipment)
    {
        if (item.IsEmpty() || item.IsBroken)
        {
            continue;
        }

        twoHandedSword |= item.Group == ItemGroupSword && item.IsTwoHanded;
        twoHandedStaff |= item.Group == ItemGroupStaff && item.IsTwoHanded;
        bow |= IsBowOrCrossbow(item);
        raven |= item.Group == ItemGroupMisc && item.Number == DarkRavenNumber;
    }

    switch (baseClass)
    {
    case CLASS_WIZARD: return twoHandedStaff;
    case CLASS_KNIGHT: return twoHandedSword;
    case CLASS_ELF: return bow;
    case CLASS_DARK: return twoHandedSword || twoHandedStaff;
    case CLASS_DARK_LORD: return raven;
    default: return false;
    }
}

CombatPowerEquipment WithItemEquipped(CombatPowerEquipment equipment, const CombatPowerItem& item, int index)
{
    equipment[index] = item;
    if (index == RightHandIndex || index == LeftHandIndex)
    {
        CombatPowerItem& otherHand = equipment[index == RightHandIndex ? LeftHandIndex : RightHandIndex];
        if (!otherHand.IsEmpty() && (item.IsTwoHanded || otherHand.IsTwoHanded) && !AreHeldTogether(item, otherHand))
        {
            otherHand = CombatPowerItem{};
        }
    }

    return equipment;
}

// Whether the class holds a one-handed weapon in its left hand too (CMyInventory::IsEquipable).
bool HoldsInLeftHand(const CombatPowerItem& item, int baseClass)
{
    switch (baseClass)
    {
    case CLASS_KNIGHT:
    case CLASS_DARK:
    case CLASS_RAGEFIGHTER:
        return !item.IsTwoHanded;
    case CLASS_SUMMONER:
        return !item.IsStaffOrBook;
    default:
        return false;
    }
}

// The hero's equipment as the item table and the item packets give it.
CombatPowerEquipment DescribeHeroEquipment()
{
    CombatPowerEquipment equipment{};
    if (CharacterMachine == nullptr)
    {
        return equipment;
    }

    for (int index = 0; index < static_cast<int>(equipment.size()) && index < MAX_EQUIPMENT_INDEX; ++index)
    {
        const ITEM& worn = CharacterMachine->Equipment[index];
        if (worn.Type >= 0 && worn.Type < MAX_ITEM)
        {
            equipment[index] = GameLogic::Items::DescribeForCombatPower(worn);
        }
    }

    return equipment;
}

// The class part of CMyInventory::IsEquipable: whether the hero's class (and class step) may wear the item, whatever
// the hero's stats and level.
bool CanHeroClassWear(int itemType)
{
    const ITEM_ATTRIBUTE& attribute = ItemAttribute[itemType];
    const auto baseClass = gCharacterManager.GetBaseClass(Hero->Class);
    const bool classMayWear = attribute.RequireClass[baseClass] != 0
                              || (baseClass == CLASS_DARK && attribute.RequireClass[CLASS_WIZARD] != 0
                                  && attribute.RequireClass[CLASS_KNIGHT] != 0);
    return classMayWear && attribute.RequireClass[baseClass] <= gCharacterManager.GetStepClass(Hero->Class);
}
} // namespace

namespace GameLogic::Items
{
CombatPowerItem DescribeForCombatPower(const ITEM& item)
{
    CombatPowerItem result;
    const Data::Items::ItemDefinition* definition = g_ItemDatabase.Find(item.Type);
    if (definition == nullptr)
    {
        return result;
    }

    result.Group = item.Type / MAX_ITEM_INDEX;
    result.Number = item.Type % MAX_ITEM_INDEX;
    result.DropLevel = definition->level;
    result.Slot = static_cast<std::uint8_t>(g_ItemDatabase.GetSlot(item.Type));
    result.Level = item.Level;
    result.OptionLevel = item.OptionLevel;
    result.ExcellentFlags = item.ExcellentFlags;
    result.HasLuck = item.HasLuck;
    result.IsTwoHanded = ItemAttribute[item.Type].TwoHand;
    result.IsStaffOrBook = IsStaffOrBookType(item.Type);
    result.IsBroken = item.Durability == 0;
    result.IsAncient = item.AncientDiscriminator > 0;
    if (result.IsAncient)
    {
        result.IsHighTierAncient =
            IsHighTierAncientSet(g_csItemOption.GetAncientSetName(item.Type, item.AncientDiscriminator));
        result.AncientSet = g_csItemOption.GetAncientSetNumber(item.Type, item.AncientDiscriminator);
        result.AncientSetSize = g_csItemOption.GetAncientSetItemCount(result.AncientSet);
    }

    return result;
}

int GetItemCombatPower(const CombatPowerItem& item)
{
    const int points = GetOwnPoints(item);
    return IsGear(item) && item.IsAncient ? points + GetAncientPiecePoints(item) : points;
}

AncientSetCombatPower GetAncientSetCombatPower(bool highTier)
{
    return highTier ? AncientSetCombatPower{600, 2500} : AncientSetCombatPower{300, 1000};
}

bool IsHighTierAncientSet(const wchar_t* setName)
{
    if (setName == nullptr)
    {
        return false;
    }

    return std::any_of(HighTierSetNames.begin(), HighTierSetNames.end(),
                       [setName](const wchar_t* name) { return std::wcscmp(name, setName) == 0; });
}

int GetEquipmentCombatPower(const CombatPowerEquipment& equipment, int baseClass)
{
    struct WornSet
    {
        int Set = 0;
        int Size = 0;
        bool IsHighTier = false;
        // The different items worn (the server counts each item of the set once).
        std::vector<std::pair<int, int>> Pieces;
    };

    int points = 0;
    std::vector<WornSet> sets;
    for (const CombatPowerItem& item : equipment)
    {
        if (item.IsEmpty() || item.IsBroken)
        {
            continue;
        }

        points += GetOwnPoints(item);
        if (!item.IsAncient || item.AncientSet == 0 || !IsGear(item))
        {
            continue;
        }

        auto set = std::find_if(sets.begin(), sets.end(), [&item](const WornSet& worn) { return worn.Set == item.AncientSet; });
        if (set == sets.end())
        {
            sets.push_back({item.AncientSet, item.AncientSetSize, item.IsHighTierAncient, {}});
            set = std::prev(sets.end());
        }

        const std::pair<int, int> piece{item.Group, item.Number};
        if (std::find(set->Pieces.begin(), set->Pieces.end(), piece) == set->Pieces.end())
        {
            set->Pieces.push_back(piece);
        }
    }

    bool isAnySetWhole = false;
    for (const WornSet& set : sets)
    {
        const int worn = static_cast<int>(set.Pieces.size());
        const int size = std::max(1, set.Size);
        const AncientSetCombatPower bonus = GetAncientSetCombatPower(set.IsHighTier);
        points += worn * (set.IsHighTier ? HighTierAncientPiecePoints : AncientPiecePoints);
        points += std::max(0, std::min(worn, size) - 1) * bonus.PerFurtherPiece;
        if (worn >= size)
        {
            points += bonus.WholeSet;
            isAnySetWhole = true;
        }
    }

    if (isAnySetWhole && HasPowerOfTheAncientsWeapon(equipment, baseClass))
    {
        points += PowerOfTheAncientsPoints;
    }

    return points;
}

std::optional<int> GetCombatPowerChange(const CombatPowerEquipment& equipment, const CombatPowerItem& item,
                                        int baseClass)
{
    if (item.IsEmpty() || item.Slot >= equipment.size())
    {
        return std::nullopt;
    }

    const int slot = item.Slot;
    if (baseClass == CLASS_RAGEFIGHTER && slot == static_cast<int>(ItemSlot::Gloves))
    {
        return std::nullopt;
    }

    std::array<int, 2> indexes{slot, -1};
    if (slot == static_cast<int>(ItemSlot::Ring))
    {
        indexes[1] = SecondRingIndex;
    }
    else if (slot == RightHandIndex && HoldsInLeftHand(item, baseClass))
    {
        indexes[1] = LeftHandIndex;
    }

    int best = INT_MIN;
    for (const int index : indexes)
    {
        if (index >= 0)
        {
            best = std::max(best, GetEquipmentCombatPower(WithItemEquipped(equipment, item, index), baseClass));
        }
    }

    return best - GetEquipmentCombatPower(equipment, baseClass);
}

std::optional<int> GetHeroCombatPowerChange(const ITEM& item)
{
    if (Hero == nullptr || CharacterMachine == nullptr || item.Type < 0 || item.Type >= MAX_ITEM)
    {
        return std::nullopt;
    }

    const ITEM* firstWorn = &CharacterMachine->Equipment[0];
    if (&item >= firstWorn && &item < firstWorn + MAX_EQUIPMENT_INDEX)
    {
        return std::nullopt;
    }

    if (!CanHeroClassWear(item.Type))
    {
        return std::nullopt;
    }

    return GetCombatPowerChange(DescribeHeroEquipment(), DescribeForCombatPower(item),
                                gCharacterManager.GetBaseClass(Hero->Class));
}
} // namespace GameLogic::Items
