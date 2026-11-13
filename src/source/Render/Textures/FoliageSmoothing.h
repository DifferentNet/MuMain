// FoliageSmoothing.h: the Remaster's smoother cut-out foliage (desktop, HD textures), from the
// player's [Render] FoliageSmoothing settings.
//
//   Grass, leaves, fences and railings are cut out of their texture by the alpha test. The Enhanced
//   samplers lean half a mip level toward the sharper one with HD textures (MipLodBias), so such a
//   cut-out edge is sampled finer than the screen can show and crawls while the camera moves.
//   Two parts, no shader change:
//   - A map's own cut-out textures (IsMapCutoutCandidatePath, not a terrain tile, and less than
//     kCutoutCoverageMax of their texels pass the alpha test) get a sampler pair of their own with a
//     less negative bias: MipLodBias + strength * kMaxBiasShift (-0.5 off, 0 at 50 %, +0.2 at 70 %,
//     +0.5 at 100 %). It adds no light to anything.
//   - With anti-aliasing (MSAA) on, the grass quads take alpha-to-coverage as map objects already do.
//   Textures under Data\Player, Monster, Item, NPC, Skill and Effect are never touched. Off, locked,
//   Low and the browser build keep the samplers and the draws as they were.
#pragma once

#include <cstdint>
#include <optional>

namespace Render::Textures::FoliageSmoothing
{

// [Render] FoliageSmoothingStrength by default.
inline constexpr int kDefaultPercent = 70;
// The mip bias added at strength 1, in mip levels.
inline constexpr float kMaxBiasShift = 1.0f;
// A texture with at least this share of its texels passing the alpha test is solid, not a cut-out.
inline constexpr float kCutoutCoverageMax = 0.90f;

// The strength for the saved settings: FoliageSmoothingStrength / 100 (0 to 1); 0 when not available
// (no HD textures, the browser build), on the Low graphics preset, for FoliageSmoothing 0 or an
// unknown value, and at strength 0. strengthPercent is clamped to 0..100.
[[nodiscard]] float Resolve(int foliageSmoothing, int strengthPercent, bool available, bool lowPreset);

// Why it does not show right now (the Options window's FoliageSmoothing row).
enum class Block : std::uint8_t
{
    None,
    // HD textures are off in this session (they switch on with the next start).
    NoHdTextures,
    // The Low graphics preset keeps it off.
    LowPreset,
};

// The first reason that applies, in the order of Block.
[[nodiscard]] Block ResolveBlock(bool hdTextures, bool lowPreset);
// Whether a reason locks the Options row.
[[nodiscard]] bool Locks(Block block);

// The cut-out samplers' mip bias at a strength (0..1): baseBias + strength * kMaxBiasShift;
// nullopt (no cut-out samplers) at strength 0 or below.
[[nodiscard]] std::optional<float> CutoutMipLodBias(float baseBias, float strength);

// Whether a texture whose alpha-test coverage (0..1, MipChain's AlphaCoverage) is this is a cut-out.
[[nodiscard]] bool IsCutout(float coverage);

// Engine side (FoliageSmoothingSetting.cpp).

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] FoliageSmoothing settings, at start-up once the graphics preset and HD textures are
// known, and again by the Options window (a change of graphics preset). Hands the cut-out mip bias
// to the textures (applied between frames); logs only when the strength changes.
void ApplyFromConfig();
// The strength in use (0..1); 0 = off.
[[nodiscard]] float Strength();
// Why it does not show right now, from the session's HD textures and graphics preset.
[[nodiscard]] Block CurrentBlock();
// Saves [Render] FoliageSmoothing and FoliageSmoothingStrength (Options > Graphics) and applies them at once.
void Choose(bool enabled, int strengthPercent);

} // namespace Render::Textures::FoliageSmoothing
