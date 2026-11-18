#pragma once

#include <cstdint>

// Junk marks (pad round 6, B7; ghinzuro 2026-11-18): on pad and touch the player marks plain gear as junk in the bag
// and sells it all at an NPC shop in one go. This file holds the pure rules: which items may ever be junk and the
// fingerprint a mark keeps, so that a mark never sells another item that came into its cell. JunkMarkSet keeps the
// marks; UI/Inventory/JunkSale reads the items and runs the sale.
namespace GameLogic::Items::JunkMarks
{
// What the item is, as far as the junk rule cares.
enum class Kind : std::uint8_t
{
    Weapon,
    Shield,
    Armour, // helm, armour, pants, gloves, boots
    Other,  // rings, pendants, wings, pets, jewels, potions, quest and event items, ...
};

// The facts about one item the rule needs; JunkItem reads them from the game's item.
struct Traits
{
    Kind kind = Kind::Other;
    int level = 0; // +0 to +15
    bool excellent = false;
    bool ancient = false;
    bool luck = false;
    bool sockets = false;
    bool harmony = false;    // a Jewel of Harmony option
    bool sellable = true;    // an NPC shop takes it at all
    bool highValue = false;  // GameLogic::Items::IsHighValueItem
    bool questOrEvent = false;
    bool mixIngredient = false; // a chaos weapon, which the wing mixes take
};

// The lowest level that is never junk (the same line as IsHighValueItem's +7).
inline constexpr int FirstValuableLevel = 7;

// Only plain weapons, shields and armour below +7 may be junk: never excellent, ancient, lucky, socketed or
// reinforced gear, never a mix ingredient, never anything the shop won't take or that the game counts as valuable. Checked when a mark is
// set and again right before each sale.
bool CanBeJunk(const Traits& traits);

// What identifies an item in its cell, without the client's own item key (it changes when the item is re-created)
// and without its durability (it wears down).
struct Identity
{
    int type = -1;
    int level = 0;
    int optionLevel = 0;
    int excellentFlags = 0;
    int ancient = 0;
    bool luck = false;
    bool skill = false;
    int harmonyOption = 0;
    int socketCount = 0;
};

using Fingerprint = std::uint64_t;

// Never 0, so 0 can stand for "no mark".
Fingerprint MakeFingerprint(const Identity& identity);
} // namespace GameLogic::Items::JunkMarks
