#include "Render/Terrain/TorchLights.h"

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_define.h"
#include "Render/Renderer/RenderUtils.h"
#include "Render/Terrain/TerrainLightGrid.h"

#include <algorithm>
#include <cmath>

namespace Render::Terrain::TorchLights
{
namespace
{
constexpr int kColorComponents = 3;
constexpr float kPercentPerUnit = 100.0f;
constexpr std::uint32_t kByteMask = 0xFFu;
constexpr int kBitsPerChannel = 8;
constexpr float kByteScale = 255.0f;
constexpr std::size_t kGridVertices = static_cast<std::size_t>(TERRAIN_SIZE) * TERRAIN_SIZE;

struct Candidate
{
    const Lighting::DynamicLight* light;
    float distance;
};

[[nodiscard]] bool Eligible(const Lighting::DynamicLight& light)
{
    return light.fromMap && light.range > 0 && light.color[0] >= 0.0f && light.color[1] >= 0.0f &&
           light.color[2] >= 0.0f;
}

[[nodiscard]] bool Darkens(const Lighting::DynamicLight& light)
{
    return light.range > 0 && (light.color[0] < 0.0f || light.color[1] < 0.0f || light.color[2] < 0.0f);
}

// Cell distance on one axis between two light centres, wrapped as LightGrid::AddLight wraps.
[[nodiscard]] int WrappedCellGap(float a, float b)
{
    const int gap = (static_cast<int>(a / TERRAIN_SCALE) - static_cast<int>(b / TERRAIN_SCALE)) & TERRAIN_SIZE_MASK;
    return (std::min)(gap, TERRAIN_SIZE - gap);
}

// True when the squares LightGrid::AddLight walks for the two lights share a vertex. The grid clamps
// each vertex at 0 after every call, so under a darkening light the removable part can be smaller
// than the share and the pool would add back light the classic picture never had.
[[nodiscard]] bool FootprintsOverlap(const Lighting::DynamicLight& a, const Lighting::DynamicLight& b)
{
    const int reach = a.range + b.range;
    return WrappedCellGap(a.position[0], b.position[0]) <= reach &&
           WrappedCellGap(a.position[1], b.position[1]) <= reach;
}

[[nodiscard]] bool UnderDarkening(std::span<const Lighting::DynamicLight> lights, const Lighting::DynamicLight& light)
{
    for (const Lighting::DynamicLight& other : lights)
    {
        if (Darkens(other) && FootprintsOverlap(light, other))
        {
            return true;
        }
    }
    return false;
}

// Keeps the kMaxLights + 1 nearest candidates, nearest first.
void Insert(std::array<Candidate, kMaxLights + 1>& nearest, std::size_t& count, const Candidate& candidate)
{
    if (count == nearest.size() && candidate.distance >= nearest[count - 1].distance)
    {
        return;
    }
    std::size_t at = count < nearest.size() ? count++ : count - 1;
    while (at > 0 && nearest[at - 1].distance > candidate.distance)
    {
        nearest[at] = nearest[at - 1];
        --at;
    }
    nearest[at] = candidate;
}

// The colour bytes mu::PackABGR gives (r, g, b), as 0..255 values.
void PackedBytes(float r, float g, float b, float bytes[3])
{
    const std::uint32_t packed = mu::PackABGR(r, g, b, 1.0f);
    for (int channel = 0; channel < kColorComponents; ++channel)
    {
        bytes[channel] = static_cast<float>((packed >> (channel * kBitsPerChannel)) & kByteMask);
    }
}

void ScaledBytes(const float light[3], const float* factor, float bytes[3])
{
    if (factor)
    {
        PackedBytes(light[0] * factor[0], light[1] * factor[1], light[2] * factor[2], bytes);
        return;
    }
    PackedBytes(light[0], light[1], light[2], bytes);
}
} // namespace

float Resolve(int torchLights, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || torchLights != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, 100)) / kPercentPerUnit;
}

float Gain(float strength)
{
    return kBaseGain + std::clamp(strength, 0.0f, 1.0f);
}

Block ResolveBlock(bool hdTextures, bool lowPreset, bool dynamicLights)
{
    if (!hdTextures)
    {
        return Block::NoHdTextures;
    }
    if (lowPreset)
    {
        return Block::LowPreset;
    }
    if (!dynamicLights)
    {
        return Block::NoDynamicLights;
    }
    return Block::None;
}

bool Locks(Block block)
{
    return block != Block::None;
}

std::size_t Select(std::span<const Lighting::DynamicLight> lights, const float reference[3], Selection& selected)
{
    std::array<Candidate, kMaxLights + 1> nearest{};
    std::size_t count = 0;
    const bool anyDarkening = std::any_of(lights.begin(), lights.end(), Darkens);
    for (const Lighting::DynamicLight& light : lights)
    {
        if (!Eligible(light))
        {
            continue;
        }
        const float dx = light.position[0] - reference[0];
        const float dy = light.position[1] - reference[1];
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance >= kMaxDistance || (anyDarkening && UnderDarkening(lights, light)))
        {
            continue;
        }
        Insert(nearest, count, {&light, distance});
    }

    // The 17th light's distance ends the set; the 16 nearer ones fade out before it.
    const float cutoff = count > kMaxLights ? (std::min)(kMaxDistance, nearest[kMaxLights].distance) : kMaxDistance;
    const std::size_t kept = (std::min)(count, kMaxLights);
    std::size_t used = 0;
    for (std::size_t i = 0; i < kept; ++i)
    {
        const float weight = std::clamp((cutoff - nearest[i].distance) / kFadeBand, 0.0f, 1.0f);
        if (weight <= 0.0f)
        {
            continue;
        }
        const Lighting::DynamicLight& light = *nearest[i].light;
        Selected& entry = selected[used++];
        entry.x = light.position[0];
        entry.y = light.position[1];
        entry.radius = light.radius * kRadiusScale;
        entry.weight = weight;
        std::copy_n(light.color, kColorComponents, entry.color);
        entry.range = light.range;
    }
    return used;
}

mu::TerrainLightsUniform Pack(std::span<const Selected> selected, float gain)
{
    mu::TerrainLightsUniform uniform{};
    const std::size_t count = (std::min)(selected.size(), kMaxLights);
    uniform.header[0] = static_cast<float>(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const Selected& light = selected[i];
        uniform.positionAndRadius[i][0] = light.x;
        uniform.positionAndRadius[i][1] = light.y;
        uniform.positionAndRadius[i][3] = light.radius;
        for (int channel = 0; channel < kColorComponents; ++channel)
        {
            uniform.color[i][channel] = light.color[channel] * light.weight * gain;
        }
    }
    return uniform;
}

void ShareGrid::Rebuild(std::span<const Selected> selected)
{
    Clear();
    if (m_cells.empty())
    {
        m_cells.assign(kGridVertices, {0.0f, 0.0f, 0.0f});
    }
    auto* grid = reinterpret_cast<float(*)[3]>(m_cells.data());
    for (const Selected& light : selected)
    {
        if (light.weight <= 0.0f || light.range <= 0)
        {
            continue;
        }
        const float share[kColorComponents] = {light.color[0] * light.weight, light.color[1] * light.weight,
                                               light.color[2] * light.weight};
        LightGrid::AddLight(grid, light.x, light.y, share, light.range);
        m_footprints.push_back({light.x, light.y, light.range});
    }
}

const float* ShareGrid::At(int terrainIndex) const
{
    static constexpr float kBlack[kColorComponents] = {0.0f, 0.0f, 0.0f};
    if (m_cells.empty() || terrainIndex < 0 || static_cast<std::size_t>(terrainIndex) >= m_cells.size())
    {
        return kBlack;
    }
    return m_cells[static_cast<std::size_t>(terrainIndex)].data();
}

void ShareGrid::Clear()
{
    // The square LightGrid::AddLight walked for each light, wrapped as it wraps.
    for (const Footprint& footprint : m_footprints)
    {
        const int xi = static_cast<int>(footprint.x / TERRAIN_SCALE);
        const int yi = static_cast<int>(footprint.y / TERRAIN_SCALE);
        for (int y = yi - footprint.range; y <= yi + footprint.range; ++y)
        {
            for (int x = xi - footprint.range; x <= xi + footprint.range; ++x)
            {
                m_cells[static_cast<std::size_t>((y & TERRAIN_SIZE_MASK) * TERRAIN_SIZE + (x & TERRAIN_SIZE_MASK))] = {
                    0.0f, 0.0f, 0.0f};
            }
        }
    }
    m_footprints.clear();
}

void RemovablePart(const float light[3], const float share[3], const float* factor, float out[3])
{
    if (share[0] == 0.0f && share[1] == 0.0f && share[2] == 0.0f)
    {
        std::fill_n(out, kColorComponents, 0.0f);
        return;
    }
    float full[kColorComponents];
    ScaledBytes(light, factor, full);
    const float baseLight[kColorComponents] = {(std::max)(light[0] - share[0], 0.0f),
                                               (std::max)(light[1] - share[1], 0.0f),
                                               (std::max)(light[2] - share[2], 0.0f)};
    float base[kColorComponents];
    ScaledBytes(baseLight, factor, base);
    for (int channel = 0; channel < kColorComponents; ++channel)
    {
        out[channel] = (std::max)(full[channel] - base[channel], 0.0f) / kByteScale;
    }
}

} // namespace Render::Terrain::TorchLights
