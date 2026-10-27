// Tests of the Combat Power an item shows in its tooltip: the numbers must be the server's (MU Moldova add-on
// CombatPower.cs), or the tooltip and /cp disagree.

#include "stdafx.h"

#include "doctest.h"

#include "GameLogic/Items/ItemCombatPower.h"

#include <initializer_list>
#include <utility>

using GameLogic::Items::CombatPowerEquipment;
using GameLogic::Items::CombatPowerItem;
using GameLogic::Items::GetAncientSetCombatPower;
using GameLogic::Items::GetCombatPowerChange;
using GameLogic::Items::GetEquipmentCombatPower;
using GameLogic::Items::GetItemCombatPower;
using GameLogic::Items::IsHighTierAncientSet;

namespace
{
// Data::Items::ItemSlot
constexpr std::uint8_t MainHand = 0;
constexpr std::uint8_t OffHand = 1;
constexpr std::uint8_t Helm = 2;
constexpr std::uint8_t Armor = 3;
constexpr std::uint8_t Pants = 4;
constexpr std::uint8_t Gloves = 5;
constexpr std::uint8_t Boots = 6;
constexpr std::uint8_t Wings = 7;
constexpr std::uint8_t Pet = 8;
constexpr std::uint8_t Ring = 10;
constexpr std::uint8_t None = 255;

// CLASS_TYPE
constexpr int Wizard = 0;
constexpr int Knight = 1;
constexpr int Elf = 2;
constexpr int RageFighter = 6;

// Equipment indexes
constexpr int RightHand = 0;
constexpr int LeftHand = 1;
constexpr int SecondRing = 11;

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

// A piece of a five-piece ancient set, 500 points of its own (drop level 50).
CombatPowerItem Piece(int set, int group, std::uint8_t slot, bool highTier = false)
{
    CombatPowerItem item = Gear(group, 1, slot, 50, 0);
    item.IsAncient = true;
    item.IsHighTierAncient = highTier;
    item.AncientSet = set;
    item.AncientSetSize = 5;
    return item;
}

CombatPowerItem TwoHanded(CombatPowerItem item)
{
    item.IsTwoHanded = true;
    return item;
}

CombatPowerEquipment Wearing(std::initializer_list<std::pair<int, CombatPowerItem>> items)
{
    CombatPowerEquipment equipment{};
    for (const auto& [index, item] : items)
    {
        equipment[index] = item;
    }

    return equipment;
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

TEST_CASE("the equipment counts each piece, every further piece of a set and the whole set")
{
    const CombatPowerEquipment twoPieces = Wearing({{2, Piece(9, 7, Helm)}, {3, Piece(9, 8, Armor)}});
    CHECK(GetEquipmentCombatPower(twoPieces, Wizard) == 2 * 500 + 2 * 250 + 300);

    const CombatPowerEquipment wholeSet = Wearing({{2, Piece(9, 7, Helm)}, {3, Piece(9, 8, Armor)}, {4, Piece(9, 9, Pants)},
                                                   {5, Piece(9, 10, Gloves)}, {6, Piece(9, 11, Boots)}});
    CHECK(GetEquipmentCombatPower(wholeSet, Wizard) == 5 * 500 + 5 * 250 + 4 * 300 + 1000);
}

TEST_CASE("a high-tier set counts twice")
{
    const CombatPowerEquipment twoPieces = Wearing({{2, Piece(30, 7, Helm, true)}, {3, Piece(30, 8, Armor, true)}});
    CHECK(GetEquipmentCombatPower(twoPieces, Wizard) == 2 * 500 + 2 * 500 + 600);
}

TEST_CASE("the same item of a set worn twice counts once for the set")
{
    CombatPowerItem ring = Piece(9, 13, Ring);
    ring.DropLevel = 0;
    const CombatPowerEquipment rings = Wearing({{10, ring}, {SecondRing, ring}});
    CHECK(GetEquipmentCombatPower(rings, Wizard) == 250);
}

TEST_CASE("Power of the Ancients needs a whole set and the class's weapon")
{
    CombatPowerEquipment equipment = Wearing({{2, Piece(9, 7, Helm)}, {3, Piece(9, 8, Armor)}, {4, Piece(9, 9, Pants)},
                                              {5, Piece(9, 10, Gloves)}, {6, Piece(9, 11, Boots)}});
    const int set = 5 * 500 + 5 * 250 + 4 * 300 + 1000;
    equipment[RightHand] = TwoHanded(Gear(0, 17, MainHand, 100, 0));
    CHECK(GetEquipmentCombatPower(equipment, Knight) == set + 1000 + 1500);
    CHECK(GetEquipmentCombatPower(equipment, Wizard) == set + 1000);
}

TEST_CASE("broken items count nothing")
{
    CombatPowerItem helm = Gear(7, 1, Helm, 50, 0);
    helm.IsBroken = true;
    CHECK(GetEquipmentCombatPower(Wearing({{2, helm}}), Wizard) == 0);
}

TEST_CASE("an item replaces the one in its slot")
{
    const CombatPowerEquipment equipment = Wearing({{2, Gear(7, 1, Helm, 30, 0)}});
    CHECK(GetCombatPowerChange(equipment, Gear(7, 2, Helm, 50, 0), Wizard) == 200);
    CHECK(GetCombatPowerChange(equipment, Gear(7, 3, Helm, 10, 0), Wizard) == -200);
    CHECK(GetCombatPowerChange(equipment, Gear(8, 3, Armor, 10, 0), Wizard) == 100);
}

TEST_CASE("a ring goes into a free ring slot, else instead of the weaker ring")
{
    const CombatPowerItem ring = Gear(13, 21, Ring, 100, 0);
    CHECK(GetCombatPowerChange(Wearing({{10, Gear(13, 9, Ring, 20, 0)}}), ring, Wizard) == 500);
    CHECK(GetCombatPowerChange(Wearing({{10, Gear(13, 9, Ring, 20, 0)}, {SecondRing, Gear(13, 8, Ring, 60, 0)}}), ring,
                               Wizard) == 400);
}

TEST_CASE("a knight holds a second one-handed weapon in the left hand, a wizard doesn't")
{
    const CombatPowerEquipment equipment = Wearing({{RightHand, Gear(0, 5, MainHand, 60, 0)}});
    CHECK(GetCombatPowerChange(equipment, Gear(0, 6, MainHand, 40, 0), Knight) == 400);
    CHECK(GetCombatPowerChange(equipment, Gear(0, 6, MainHand, 40, 0), Wizard) == -200);
}

TEST_CASE("a two-handed weapon takes both hands, a bow keeps its arrows")
{
    const CombatPowerEquipment swordAndShield =
        Wearing({{RightHand, Gear(0, 5, MainHand, 60, 0)}, {LeftHand, Gear(6, 4, OffHand, 50, 0)}});
    CHECK(GetCombatPowerChange(swordAndShield, TwoHanded(Gear(0, 17, MainHand, 100, 0)), Knight) == 1000 - 1100);

    const CombatPowerEquipment twoHandedSword = Wearing({{RightHand, TwoHanded(Gear(0, 17, MainHand, 100, 0))}});
    CHECK(GetCombatPowerChange(twoHandedSword, Gear(6, 4, OffHand, 50, 0), Knight) == 500 - 1000);

    CombatPowerItem arrows = Gear(4, 15, MainHand, 0, 2);
    const CombatPowerEquipment bowAndArrows = Wearing({{LeftHand, TwoHanded(Gear(4, 17, OffHand, 92, 0))}, {RightHand, arrows}});
    CHECK(GetCombatPowerChange(bowAndArrows, TwoHanded(Gear(4, 20, OffHand, 135, 0)), Elf) == 430);
}

TEST_CASE("the last piece of a set adds the whole set")
{
    const CombatPowerEquipment fourPieces = Wearing({{2, Piece(9, 7, Helm)}, {3, Piece(9, 8, Armor)},
                                                     {4, Piece(9, 9, Pants)}, {5, Piece(9, 10, Gloves)}});
    CHECK(GetCombatPowerChange(fourPieces, Piece(9, 11, Boots), Wizard) == 500 + 250 + 300 + 1000);
}

TEST_CASE("items that can't be equipped have no change")
{
    CHECK_FALSE(GetCombatPowerChange(CombatPowerEquipment{}, Gear(14, 13, None, 0, 0), Wizard).has_value());
    CHECK_FALSE(GetCombatPowerChange(CombatPowerEquipment{}, Gear(8, 3, Gloves, 10, 0), RageFighter).has_value());
}
