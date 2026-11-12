#include "Render/Terrain/TerrainDetail.h"

#include <algorithm>

namespace Render::Terrain::Detail
{

Params Resolve(int variant, int strengthPercent, bool available)
{
    const float strength = std::clamp(static_cast<float>(strengthPercent) / kPercentPerUnit, 0.0f, 1.0f);
    if (!available || strength <= 0.0f)
        return {};

    switch (static_cast<Variant>(variant))
    {
    case Variant::SelfDetail:
        return {strength, kSelfDetailScale};
    case Variant::Noise:
        return {strength, -kNoiseScale};
    default:
        return {};
    }
}

} // namespace Render::Terrain::Detail
