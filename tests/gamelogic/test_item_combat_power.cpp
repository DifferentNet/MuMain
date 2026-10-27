// Tests of the Combat Power an item shows in its tooltip: the numbers must be the server's (MU Moldova add-on
// CombatPower.cs), or the tooltip and /cp disagree.

#include "stdafx.h"

#include "doctest.h"

#include "GameLogic/Items/ItemCombatPower.h"

using GameLogic::Items::CombatPowerItem;
using GameLogic::Items::GetAncientSetCombatPower;
using GameLogic::Items::GetItemCombatPower;
using GameLogic::Items::IsHighTierAncientSet;

namespace
{
// Data::Items::ItemSlot
constexpr std::uint8_t MainHand = 0;
constexpr std::uint8_t OffHand = 1;
constexpr std::uint8_t Helm = 2;
constexpr std::uint8_t Armor = 3;
constexpr std::uint8_t Wings = 7;
constexpr std::uint8_t Pet = 8;
constexpr std::uint8_t Ring = 10;
constexpr std::uint8_t None = 255;

CombatPowerItem Gear(int group, int number, std::uint8_t slot, int dropLevel, int level)
{
    CombatPowerItem item;
    item.Group = group;
    item.Number = number;
    item.Slot = slot;
    item.DropLevel = dropLevel;
    item.Level = level;
    return item;
}

CombatPowerItem Misc(int group, int number, std::uint8_t slot, std::uint8_t excellentFlags = 0)
{
    CombatPowerItem item = Gear(group, number, slot, 0, 0);
    item.ExcellentFlags = excellentFlags;
    return item;
}
} // namespace

TEST_CASE("armour counts its drop level, level, excellent options, Jewel of Life steps and luck")
{
    CombatPowerItem armour = Gear(8, 7, Armor, 100, 13);
    armour.OptionLevel = 4;
    armour.ExcellentFlags = 0x03;
    armour.HasLuck = true;
    CHECK(GetItemCombatPower(armour) == 1000 + 900 + 350 + 80 + 50);
}

TEST_CASE("pendants and rings count half their drop level")
{
    CombatPowerItem ring = Gear(13, 21, Ring, 70, 0);
    ring.OptionLevel = 1;
    ring.ExcellentFlags = 0x01;
    CHECK(GetItemCombatPower(ring) == 350 + 20 + 150);
}

TEST_CASE("item levels and excellent options stop at the table's end")
{
    CombatPowerItem sword = Gear(0, 0, MainHand, 0, 20);
    sword.ExcellentFlags = 0xFF;
    CHECK(GetItemCombatPower(sword) == 1500 + 1500);
}

TEST_CASE("an ancient piece adds 250, a high-tier piece 500")
{
    CombatPowerItem helm = Gear(7, 1, Helm, 50, 9);
    helm.IsAncient = true;
    CHECK(GetItemCombatPower(helm) == 500 + 250 + 250);
    helm.IsHighTierAncient = true;
    CHECK(GetItemCombatPower(helm) == 500 + 250 + 500);
}

TEST_CASE("items whose client level differs use the server's")
{
    CHECK(GetItemCombatPower(Gear(7, 44, Helm, 116, 0)) == 1100);
    CHECK(GetItemCombatPower(Gear(13, 76, Ring, 1, 0)) == 140);
}

TEST_CASE("wings count their kind and twice their level")
{
    CHECK(GetItemCombatPower(Gear(12, 0, Wings, 100, 9)) == 500 + 500);
    CHECK(GetItemCombatPower(Gear(13, 30, Wings, 180, 0)) == 1500);
    CHECK(GetItemCombatPower(Gear(12, 36, Wings, 380, 13)) == 4000 + 1800);
    CHECK(GetItemCombatPower(Gear(12, 43, Wings, 380, 0)) == 4000);
    CHECK(GetItemCombatPower(Gear(12, 49, Wings, 180, 9)) == 0);
}

TEST_CASE("pets count by kind, the others nothing")
{
    CHECK(GetItemCombatPower(Misc(13, 37, Pet)) == 500);
    CHECK(GetItemCombatPower(Misc(13, 37, Pet, 0x02)) == 800);
    CHECK(GetItemCombatPower(Misc(13, 37, Pet, 0x01)) == 1000);
    CHECK(GetItemCombatPower(Misc(13, 37, Pet, 0x04)) == 1000);
    CHECK(GetItemCombatPower(Misc(13, 3, Pet)) == 200);
    CHECK(GetItemCombatPower(Misc(13, 4, Pet)) == 700);
    CHECK(GetItemCombatPower(Misc(13, 0, Pet)) == 0);
    CHECK(GetItemCombatPower(Misc(13, 5, OffHand)) == 0);
}

TEST_CASE("items that can't be equipped have none")
{
    CHECK(GetItemCombatPower(Gear(14, 13, None, 0, 0)) == 0);
}

TEST_CASE("the high-tier sets and what a set adds")
{
    CHECK(IsHighTierAncientSet(L"Bragi"));
    CHECK(IsHighTierAncientSet(L"Surt"));
    CHECK_FALSE(IsHighTierAncientSet(L"Enis"));
    CHECK_FALSE(IsHighTierAncientSet(nullptr));
    CHECK(GetAncientSetCombatPower(false).PerFurtherPiece == 300);
    CHECK(GetAncientSetCombatPower(false).WholeSet == 1000);
    CHECK(GetAncientSetCombatPower(true).PerFurtherPiece == 600);
    CHECK(GetAncientSetCombatPower(true).WholeSet == 2500);
}
