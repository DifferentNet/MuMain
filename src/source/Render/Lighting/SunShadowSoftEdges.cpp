#include "Render/Lighting/SunShadowSoftEdges.h"

#include <algorithm>

namespace Render::Lighting::SunShadowSoftEdges
{
namespace
{
constexpr int kMaxPercent = 100;
constexpr float kPercentPerUnit = 100.0f;

[[nodiscard]] float ClampStrength(float strength)
{
    return std::clamp(strength, 0.0f, 1.0f);
}
} // namespace

float Resolve(int sunShadowSoftEdges, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || sunShadowSoftEdges != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, kMaxPercent)) / kPercentPerUnit;
}

Block ResolveBlock(bool hdTextures, bool lowPreset, bool sunShadows)
{
    if (!hdTextures)
    {
        return Block::NoHdTextures;
    }
    if (lowPreset)
    {
        return Block::LowPreset;
    }
    if (!sunShadows)
    {
        return Block::NoSunShadows;
    }
    return Block::None;
}

bool Locks(Block block)
{
    return block != Block::None;
}

float Slope(float strength)
{
    return ClampStrength(strength) * kMaxSlope;
}

float GrowthWorld(float strength)
{
    return ClampStrength(strength) * kMaxGrowthWorld;
}

void ApplyToFrame(mu::SunShadowFrame& frame, float strength)
{
    if (strength <= 0.0f)
    {
        return;
    }
    frame.penumbraSlope = Slope(strength);
    frame.penumbraGrowth = GrowthWorld(strength);
}

} // namespace Render::Lighting::SunShadowSoftEdges
