#include "Render/Terrain/TerrainVariation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::Terrain::Variation
{
namespace
{
// A value in [0, 1) for an integer lattice cell and a channel.
float Hash(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 1597334673u;
    h ^= static_cast<std::uint32_t>(y) * 3812015801u;
    h ^= static_cast<std::uint32_t>(seed) * 2798796415u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

// Smooth value noise in [0, 1) at (x, y) measured in cells.
float ValueNoise(float x, float y, int seed)
{
    const float cx = std::floor(x);
    const float cy = std::floor(y);
    const int ix = static_cast<int>(cx);
    const int iy = static_cast<int>(cy);
    float fx = x - cx;
    float fy = y - cy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    const float a = Hash(ix, iy, seed);
    const float b = Hash(ix + 1, iy, seed);
    const float c = Hash(ix, iy + 1, seed);
    const float d = Hash(ix + 1, iy + 1, seed);
    const float top = a + (b - a) * fx;
    const float bottom = c + (d - c) * fx;
    return top + (bottom - top) * fy;
}

// Two octaves around 0, about -1..1.
float Macro(int x, int y, int seed)
{
    const float coarse = ValueNoise(x / kMacroCellTiles, y / kMacroCellTiles, seed);
    const float fine = ValueNoise(x / kMacroDetailCellTiles, y / kMacroDetailCellTiles, seed + 7919);
    return (kMacroWeight * coarse + kMacroDetailWeight * fine - 0.5f) * 2.0f;
}

constexpr int kBrightnessChannel = 0x51;
constexpr int kWarmthChannel = 0xA7;
constexpr int kFlipChannel = 0x3D;
} // namespace

Params Resolve(int variant, int strengthPercent, bool available)
{
    const float strength = std::clamp(static_cast<float>(strengthPercent) / kPercentPerUnit, 0.0f, 1.0f);
    if (!available || strength <= 0.0f)
        return {};

    switch (static_cast<Variant>(variant))
    {
    case Variant::Macro:
        return {true, false, strength};
    case Variant::MacroAndFlips:
        return {true, true, strength};
    default:
        return {};
    }
}

void MacroFactor(int x, int y, int seed, float strength, float out[3])
{
    const float brightness = 1.0f + strength * kBrightnessSwing * Macro(x, y, seed * 131 + kBrightnessChannel);
    const float warmth = strength * kWarmthSwing * Macro(x, y, seed * 131 + kWarmthChannel);
    out[0] = brightness * (1.0f + warmth);
    out[1] = brightness;
    out[2] = brightness * (1.0f - warmth);
}

Flip BlockFlip(int bx, int by, int texture, int seed)
{
    const float h = Hash(bx, by, seed * 257 + texture * 31 + kFlipChannel);
    const int pick = static_cast<int>(h * 4.0f); // 0..3, equally likely
    return {(pick & 1) != 0, (pick & 2) != 0};
}

void MirrorTileUv(float uv[4][2], Flip flip)
{
    const bool axes[2] = {flip.u, flip.v};
    for (int axis = 0; axis < 2; ++axis)
    {
        if (!axes[axis])
            continue;
        // The repeat holding the tile: from its smallest coordinate (a tile never straddles two).
        float low = uv[0][axis];
        for (int i = 1; i < 4; ++i)
            low = std::min(low, uv[i][axis]);
        const float origin = std::floor(low + 1.0e-4f);
        for (int i = 0; i < 4; ++i)
            uv[i][axis] = 2.0f * origin + 1.0f - uv[i][axis];
    }
}

} // namespace Render::Terrain::Variation
