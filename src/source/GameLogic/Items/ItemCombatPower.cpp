#include "stdafx.h"
#include "GameLogic/Items/ItemCombatPower.h"

#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Data/GameData/ItemData/ItemDefinition.h"
#include "GameLogic/Items/CSItemOption.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cwchar>

namespace
{
using Data::Items::ItemSlot;
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

constexpr int ItemGroupWings = 12;
constexpr int ItemGroupMisc = 13;
constexpr int DarkRavenNumber = 5;

// Fenrir's kind is in the excellent flags, as the item tooltip reads it.
constexpr int FenrirNumber = 37;
constexpr std::uint8_t BlackFenrirFlag = 0x01;
constexpr std::uint8_t BlueFenrirFlag = 0x02;
constexpr std::uint8_t GoldFenrirFlag = 0x04;

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

    if (item.IsAncient)
    {
        points += item.IsHighTierAncient ? HighTierAncientPiecePoints : AncientPiecePoints;
    }

    return points;
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
    result.IsAncient = item.AncientDiscriminator > 0;
    result.IsHighTierAncient = result.IsAncient
                               && IsHighTierAncientSet(g_csItemOption.GetAncientSetName(item.Type, item.AncientDiscriminator));
    return result;
}

int GetItemCombatPower(const CombatPowerItem& item)
{
    if (item.Slot == static_cast<std::uint8_t>(ItemSlot::Wings))
    {
        return GetWingPoints(item);
    }

    if (item.Slot == static_cast<std::uint8_t>(ItemSlot::Pet))
    {
        return GetPetPoints(item);
    }

    if (item.Slot == static_cast<std::uint8_t>(ItemSlot::None)
        || (item.Group == ItemGroupMisc && item.Number == DarkRavenNumber))
    {
        return 0;
    }

    return GetGearPoints(item);
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
} // namespace GameLogic::Items
