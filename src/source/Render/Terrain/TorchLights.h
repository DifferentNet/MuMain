// TorchLights.h: the Remaster's torch light on the ground (desktop, HD textures, Enhanced dynamic
// lights), from the player's [Render] TorchLights settings.
//
//   The game's map lights (torches, lamps, fires, street lights: the light calls map objects make,
//   DynamicLight::fromMap) light the terrain through its light grid, one colour per vertex 100
//   units apart: a blocky diamond. Each frame the kMaxLights map lights nearest to the hero are
//   taken out of the ground's vertex colour again and drawn per pixel instead, as a smooth round
//   pool (terrain_lights.hlsli): colour * gain * smooth(1 - d / radius), d the horizontal distance,
//   radius the light's range * kRadiusScale. A light's weight fades out over kFadeBand before the
//   cutoff (the 17th light's distance, at most kMaxDistance); the rest of a faded light stays in
//   the vertex colour, so nothing pops when the set changes.
//   Only the ground's opaque runs and its non-blended layers are drawn so; water, grass, blended
//   layers, objects, characters, monsters and gear keep exactly the light they had.
//   Off, locked, the browser build and a frame without map lights near the hero leave the vertex
//   data and the picture as they were.
#pragma once

#include "Render/Lighting/DynamicLight.h"
#include "Render/Renderer/SceneLighting.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Render::Terrain::TorchLights
{

// The most lights drawn per pixel.
inline constexpr std::size_t kMaxLights = mu::kMaxTerrainLights;
// Lights farther from the hero than this (world units, a tile is 100) stay in the vertex colour.
inline constexpr float kMaxDistance = 3200.0f;
// A light's weight falls from 1 to 0 over this distance before the cutoff.
inline constexpr float kFadeBand = 400.0f;
// The pool's radius is the light's range (in world units) times this.
inline constexpr float kRadiusScale = 1.15f;
// [Render] TorchLightsStrength at which the gain is kDefaultGain (1.2 times the Classic light).
inline constexpr int kDefaultPercent = 70;
// The gain is kBaseGain + the strength (0..1).
inline constexpr float kBaseGain = 0.5f;

// The strength for the saved settings: TorchLightsStrength / 100 (0 to 1); 0 when not available
// (no HD textures, the browser build), on the Low graphics preset, for TorchLights 0 or an unknown
// value, and at strength 0. strengthPercent is clamped to 0..100.
[[nodiscard]] float Resolve(int torchLights, int strengthPercent, bool available, bool lowPreset);

// The per-pixel light's multiplier at a strength (0..1): kBaseGain + strength.
[[nodiscard]] float Gain(float strength);

// Why it does not show right now (the Options window's TorchLights row).
enum class Block : std::uint8_t
{
    None,
    // HD textures are off in this session (they switch on with the next start).
    NoHdTextures,
    // The Low graphics preset keeps it off.
    LowPreset,
    // It needs Enhanced lighting with dynamic lights, both drawn.
    NoDynamicLights,
};

// The first reason that applies, in the order of Block.
[[nodiscard]] Block ResolveBlock(bool hdTextures, bool lowPreset, bool dynamicLights);
// Whether a reason locks the Options row.
[[nodiscard]] bool Locks(Block block);

// A map light drawn per pixel this frame.
struct Selected
{
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f; // the pool's radius in world units (range * 100 * kRadiusScale)
    float weight = 0.0f; // 0..1: the share taken out of the vertex colour and drawn per pixel
    float color[3]{};
    int range = 0; // in terrain cells, as the light call's
};

using Selection = std::array<Selected, kMaxLights>;

// The frame's map lights (fromMap, no darkening channel, a range) nearest to `reference` (the
// hero; horizontal distance), nearest first, with their weights; lights of weight 0 are left out.
// A light whose grid square overlaps a darkening frame light's square is left out too: the grid
// clamps at 0 there, so the pool could give back more than the grid really took away.
// Returns how many were selected.
[[nodiscard]] std::size_t Select(std::span<const Lighting::DynamicLight> lights, const float reference[3],
                                 Selection& selected);

// The shader's block for the selected lights: colours times weight and gain.
[[nodiscard]] mu::TerrainLightsUniform Pack(std::span<const Selected> selected, float gain);

// The selected lights' share of the terrain light grid: colour * weight added with the grid's own
// arithmetic (LightGrid::AddLight), so a light of weight 1 adds exactly what it added to the grid.
// The vertices last touched are cleared on the next rebuild (no full clear per frame).
class ShareGrid
{
public:
    // Clears the last frame's lights and adds these.
    void Rebuild(std::span<const Selected> selected);
    // The share at a grid vertex (y * TERRAIN_SIZE + x); black before the first rebuild.
    [[nodiscard]] const float* At(int terrainIndex) const;
    // Clears every light added.
    void Clear();

private:
    struct Footprint
    {
        float x;
        float y;
        int range;
    };
    std::vector<std::array<float, 3>> m_cells;
    std::vector<Footprint> m_footprints;
};

// What a flagged ground vertex carries in its normal: the part of its packed colour that comes from
// the selected lights, per channel 0..1. light is the vertex's grid light, share its ShareGrid
// share, factor its macro colour factor (nullptr when there is none). The colour bytes minus
// out * 255 are exactly the bytes mu::PackABGR gives max(light - share, 0) * factor.
void RemovablePart(const float light[3], const float share[3], const float* factor, float out[3]);

// Engine side (TorchLightsSetting.cpp).

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] TorchLights settings, at start-up once the graphics preset and HD textures are known,
// and again by the Options window (a change of graphics preset). Logs only when the strength changes.
void ApplyFromConfig();
// The strength in use (0..1); 0 = off.
[[nodiscard]] float Strength();
// Why it does not show right now, from the session's HD textures, graphics preset and dynamic lights.
[[nodiscard]] Block CurrentBlock();
// Saves [Render] TorchLights and TorchLightsStrength (Options > Graphics) and applies them at once.
void Choose(bool enabled, int strengthPercent);
// Picks this frame's lights, right after the frame's dynamic terrain lights were added
// (AddFrameHistoryLights), and gives them to the renderer; clears them when it does not apply.
void BuildFrame();
// Whether this frame's ground draws take lights out of their vertex colour.
[[nodiscard]] bool FrameActive();
// The share of the selected lights at a grid vertex (valid while FrameActive()).
[[nodiscard]] const float* Share(int terrainIndex);

} // namespace Render::Terrain::TorchLights
