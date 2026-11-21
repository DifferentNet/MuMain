#pragma once

#include <cstdint>

// An item's rarity as the tooltip name colour has always shown it, as one value the UI can style by (step C of the
// Skynet theme, "Сокровищница": rich item tooltips and rarity cell frames). This file holds the pure rules; the facts
// come from the item in ZzzInventory.cpp (DescribeForRarity).
namespace GameLogic::Items::Rarity
{
enum class Tier : std::uint8_t
{
    Normal,
    Magic,
    Fine,
    Excellent,
    Ancient,
    Socket,
    Special,
    Divine,
};

// +0..+6, +7..+12, +13..+15.
enum class UpgradeBand : std::uint8_t
{
    None,
    Plus7,
    Gold,
};

// The tooltip name colours a tier maps to (TEXT_COLOR_* in the engine).
enum class NameColor : std::uint8_t
{
    White,
    Blue,
    Yellow,
    Green,
    GreenBlue,
    Violet,
    Purple,
};

// What the classification reads from an item. Each flag is one condition of the old name colour chain.
struct Facts
{
    bool namedSpecial = false;     // jewels, compiled gems, 3rd class quest items, condor parts, tickets
    bool divine = false;           // Divine Archangel weapons
    bool eventPart = false;        // Devil Square items, Blood Castle ticket parts
    bool ancient = false;          // AncientDiscriminator > 0
    bool socket = false;           // a socket item
    bool excellentOptions = false; // ExcellentFlags > 0
    bool hasOptions = false;       // SpecialNum > 0
    bool wingLike = false;         // wings and capes: their colour follows only level and options
    bool jewelGem = false;         // a jewel the combination machine knows (COMGEM::Check_Jewel)
    int level = 0;
};

// The first match wins: named special, divine, event part, ancient, socket, excellent, +7, options, plain. Wings and
// capes then fall back to +7 / options / plain, and a jewel gem is always special.
[[nodiscard]] Tier Classify(const Facts& facts);

[[nodiscard]] NameColor ColorOf(Tier tier);

// The tooltip may recolour the name later (Fenrir, seeds, named items). Keeps `initial` when the colour did not
// change, otherwise the tier that colour stands for.
[[nodiscard]] Tier FromFinalNameColor(Tier initial, NameColor finalColor);

[[nodiscard]] UpgradeBand BandOf(int level);

// What an item's inventory cells show (ItemRarityCells in theme.ini): its tier and its upgrade band, each Normal /
// None for an item that shows none.
struct CellLook
{
    Tier tier = Tier::Normal;
    UpgradeBand band = UpgradeBand::None;
};
} // namespace GameLogic::Items::Rarity
