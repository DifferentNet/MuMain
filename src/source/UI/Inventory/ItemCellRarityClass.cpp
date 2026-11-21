#include "UI/Inventory/ItemCellRarityClass.h"

#include <array>
#include <cstddef>

namespace UI::Inventory
{
namespace
{
using GameLogic::Items::Rarity::Tier;
using GameLogic::Items::Rarity::UpgradeBand;

// The tinting rarities, then the bands.
enum class Tint : std::size_t
{
    None,
    Excellent,
    Ancient,
    Socket,
    Count,
};
constexpr std::size_t BandCount = 3;

// [tint][band]: band None, Plus7, Gold.
constexpr std::array<std::array<const char*, BandCount>, static_cast<std::size_t>(Tint::Count)> RarityClasses = {{
    {"", " rf up-fine", " rf up-gold"},
    {" rar-exc rf", " rar-exc rf up-fine", " rar-exc rf up-gold"},
    {" rar-anc rf", " rar-anc rf up-fine", " rar-anc rf up-gold"},
    {" rar-socket rf", " rar-socket rf up-fine", " rar-socket rf up-gold"},
}};

// Indexed by top | bottom << 1 | left << 2 | right << 3.
constexpr std::array<const char*, 16> EdgeTable = {
    "",           " re-t",           " re-b",           " re-t re-b",           " re-l",      " re-t re-l",
    " re-b re-l", " re-t re-b re-l", " re-r",           " re-t re-r",           " re-b re-r", " re-t re-b re-r",
    " re-l re-r", " re-t re-l re-r", " re-b re-l re-r", " re-t re-b re-l re-r",
};

Tint TintOf(Tier tier)
{
    switch (tier)
    {
    case Tier::Excellent:
        return Tint::Excellent;
    case Tier::Ancient:
        return Tint::Ancient;
    case Tier::Socket:
        return Tint::Socket;
    default:
        return Tint::None;
    }
}

std::size_t BandIndex(UpgradeBand band)
{
    switch (band)
    {
    case UpgradeBand::Plus7:
        return 1;
    case UpgradeBand::Gold:
        return 2;
    default:
        return 0;
    }
}
} // namespace

const char* RarityCellClasses(Tier tier, UpgradeBand band)
{
    return RarityClasses[static_cast<std::size_t>(TintOf(tier))][BandIndex(band)];
}

const char* EdgeClasses(bool top, bool bottom, bool left, bool right)
{
    const std::size_t index = (top ? 1u : 0u) | (bottom ? 2u : 0u) | (left ? 4u : 0u) | (right ? 8u : 0u);
    return EdgeTable[index];
}
} // namespace UI::Inventory
