#include "Render/Textures/FoliageSmoothing.h"

#include <algorithm>

namespace Render::Textures::FoliageSmoothing
{
namespace
{
constexpr int kMaxPercent = 100;
constexpr float kPercentPerUnit = 100.0f;
} // namespace

float Resolve(int foliageSmoothing, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || foliageSmoothing != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, kMaxPercent)) / kPercentPerUnit;
}

Block ResolveBlock(bool hdTextures, bool lowPreset)
{
    if (!hdTextures)
    {
        return Block::NoHdTextures;
    }
    if (lowPreset)
    {
        return Block::LowPreset;
    }
    return Block::None;
}

bool Locks(Block block)
{
    return block != Block::None;
}

std::optional<float> CutoutMipLodBias(float baseBias, float strength)
{
    if (strength <= 0.0f)
    {
        return std::nullopt;
    }
    return baseBias + std::min(strength, 1.0f) * kMaxBiasShift;
}

bool IsCutout(float coverage)
{
    return coverage < kCutoutCoverageMax;
}

} // namespace Render::Textures::FoliageSmoothing
