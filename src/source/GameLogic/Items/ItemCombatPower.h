#pragma once

#include <cstdint>

typedef struct tagITEM ITEM;

// The Combat Power (CP) an equipped item adds, by the server's formula (MU Moldova add-on CombatPower.cs, version 2
// of the design doc): the item tooltip shows it so that players see what an item is worth for their CP, the Tier
// Maps' entry rule and the ranking.
namespace GameLogic::Items
{
// What the Combat Power of an item depends on.
struct CombatPowerItem
{
    int Group = -1;
    int Number = -1;
    // The item table's level (OpenMU's drop level) and slot (Data::Items::ItemSlot).
    int DropLevel = 0;
    std::uint8_t Slot = 255;
    // The item's own level +0 to +15, its Jewel of Life option steps and its excellent options (for Fenrir its kind).
    int Level = 0;
    int OptionLevel = 0;
    std::uint8_t ExcellentFlags = 0;
    bool HasLuck = false;
    bool IsAncient = false;
    // A piece of one of the high-tier ancient sets (Mahes, Bragi, …), worth twice a classic piece.
    bool IsHighTierAncient = false;
};

// The points the set of an ancient piece adds on top of its pieces.
struct AncientSetCombatPower
{
    // For every piece of the set worn after the first.
    int PerFurtherPiece = 0;
    // For the whole set.
    int WholeSet = 0;
};

// Reads an item of the inventory or a shop: its table entry and, for an ancient piece, its set.
CombatPowerItem DescribeForCombatPower(const ITEM& item);

// The item's Combat Power while it is equipped and has durability left: weapons, shields, armour, pendants and
// rings, wings and pets; an ancient piece adds its own share, not its set's (see GetAncientSetCombatPower). 0 for
// items the formula leaves out (Guardian Angel, Imp, Uniria, Dark Raven, other pets and wings).
int GetItemCombatPower(const CombatPowerItem& item);

AncientSetCombatPower GetAncientSetCombatPower(bool highTier);

// Whether the ancient set of that name is one of the high-tier sets.
bool IsHighTierAncientSet(const wchar_t* setName);
} // namespace GameLogic::Items
