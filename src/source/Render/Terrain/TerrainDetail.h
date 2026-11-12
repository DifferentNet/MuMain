// TerrainDetail.h: the grain the Remaster draws over the ground near the camera
// (src/shaders/terrain_detail.hlsli), from the player's [Render] TerrainDetail settings.
//
// Ground tile UVs run on across tiles (FaceTexture: tile x * 64 / texture width), so a 256-wide
// tile texture repeats every 4 tiles (400 world units) and any uv * scale stays seamless.
#pragma once

namespace Render::Terrain::Detail
{

// [Render] TerrainDetail values.
enum class Variant
{
    Off = 0,
    SelfDetail = 1, // the tile texture sampled kSelfDetailScale times denser
    Noise = 2,      // value noise, kNoiseScale cells per texture repeat
};

// Self-detail: 4 times the tile texture's own repeat, once per tile for a 256-wide texture.
inline constexpr float kSelfDetailScale = 4.0f;
// Noise: 16 cells per texture repeat, cells of 25 world units for a 256-wide texture.
inline constexpr float kNoiseScale = 16.0f;
inline constexpr float kPercentPerUnit = 100.0f;

// What the renderer gets (IMuRenderer::SetTerrainDetail): strength 0..1, scale > 0 self-detail,
// < 0 noise; strength 0 is off.
struct Params
{
    float strength = 0.0f;
    float scale = 0.0f;
};

// The params for the saved settings; off when the grain isn't available (no HD textures, the
// browser build), for variant 0 or an unknown one, and at strength 0. strengthPercent is clamped.
[[nodiscard]] Params Resolve(int variant, int strengthPercent, bool available);

} // namespace Render::Terrain::Detail
