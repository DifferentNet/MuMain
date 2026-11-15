#include "GameLogic/Items/DurabilityBand.h"

namespace GameLogic::Items
{
namespace
{
constexpr float kHalf = 0.5f;
constexpr float kThird = 0.3f;
constexpr float kFifth = 0.2f;
} // namespace

DurabilityBand ClassifyDurability(int durability, int maxDurability)
{
    if (durability <= 0)
        return DurabilityBand::Zero;
    if (durability <= maxDurability * kFifth)
        return DurabilityBand::Fifth;
    if (durability <= maxDurability * kThird)
        return DurabilityBand::Third;
    if (durability <= maxDurability * kHalf)
        return DurabilityBand::Half;
    return DurabilityBand::Fine;
}

const char* DurabilityBandName(DurabilityBand band)
{
    switch (band)
    {
    case DurabilityBand::Half: return "half";
    case DurabilityBand::Third: return "third";
    case DurabilityBand::Fifth: return "fifth";
    case DurabilityBand::Zero: return "zero";
    case DurabilityBand::Fine: default: return "";
    }
}
} // namespace GameLogic::Items
