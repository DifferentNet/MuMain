#pragma once

#include <array>
#include <cstdint>
#include <optional>

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
    // The ancient piece's set (0: none) and how many pieces that set has.
    int AncientSet = 0;
    int AncientSetSize = 0;
    bool IsTwoHanded = false;
    // A staff, stick or book: the summoner holds those in one hand only.
    bool IsStaffOrBook = false;
    // No durability left: the server counts nothing for it until it is repaired.
    bool IsBroken = false;

    bool IsEmpty() const { return Group < 0; }
};

// The points the set of an ancient piece adds on top of its pieces.
struct AncientSetCombatPower
{
    // For every piece of the set worn after the first.
    int PerFurtherPiece = 0;
    // For the whole set.
    int WholeSet = 0;
};

// A character's equipment by the equipment indexes EQUIPMENT_WEAPON_RIGHT (0) to EQUIPMENT_RING_LEFT (11); an empty
// slot holds an empty CombatPowerItem.
using CombatPowerEquipment = std::array<CombatPowerItem, 12>;

// Reads an item of the inventory or a shop: its table entry and, for an ancient piece, its set.
CombatPowerItem DescribeForCombatPower(const ITEM& item);

// The item's Combat Power while it is equipped and has durability left: weapons, shields, armour, pendants and
// rings, wings and pets; an ancient piece adds its own share, not its set's (see GetAncientSetCombatPower). 0 for
// items the formula leaves out (Guardian Angel, Imp, Uniria, Dark Raven, other pets and wings).
int GetItemCombatPower(const CombatPowerItem& item);

AncientSetCombatPower GetAncientSetCombatPower(bool highTier);

// Whether the ancient set of that name is one of the high-tier sets.
bool IsHighTierAncientSet(const wchar_t* setName);

// The equipment's Combat Power as the server's /cp counts it, without the stats: items, ancient sets (with Power of
// the Ancients), wings and pets; broken items count nothing. baseClass: the character's CLASS_TYPE, CLASS_WIZARD (0)
// to CLASS_RAGEFIGHTER (6), which decides the weapon Power of the Ancients needs.
int GetEquipmentCombatPower(const CombatPowerEquipment& equipment, int baseClass);

// How much the equipment's Combat Power changes when the item is put on: where it fits two places (a ring, a
// one-handed weapon of a class that holds two), the better one; a two-handed weapon takes the other hand's item off,
// and so does any weapon or shield next to a two-handed one (a bow keeps its arrows, a crossbow its bolts). nullopt
// for an item that can't be equipped at all; whether the class may wear it is the caller's to check.
std::optional<int> GetCombatPowerChange(const CombatPowerEquipment& equipment, const CombatPowerItem& item,
                                        int baseClass);

// GetCombatPowerChange for the hero's own equipment; nullopt when there's no hero, the item is one the hero wears or
// the hero's class can't wear it.
std::optional<int> GetHeroCombatPowerChange(const ITEM& item);
} // namespace GameLogic::Items
