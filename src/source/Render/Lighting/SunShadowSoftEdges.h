// SunShadowSoftEdges.h: the Remaster's soft sun-shadow edges (desktop, HD textures, sun shadows on),
// from the player's [Render] SunShadowSoftEdges settings.
//
//   The sun shadow's edge has a fixed radius (the map's sunSoftness, a few world units). With this on,
//   the edge is crisp next to what casts the shadow and softer farther away from it, as a real sun's
//   shadow is: the receiver shader (sun_shadow.hlsli) looks for the casters around each shadowed pixel
//   in the shadow map and widens the edge by kMaxSlope * strength world units per world unit between
//   the caster and the pixel, at most kMaxGrowthWorld * strength world units over the map's own edge.
//   Only the shadows' edges change; no light term does. Off, locked, Low, sun shadows off and the
//   browser build keep the fixed edge.
#pragma once

#include "Render/Renderer/SunShadow.h"

#include <cstdint>

namespace Render::Lighting::SunShadowSoftEdges
{

// [Render] SunShadowSoftEdgesStrength by default.
inline constexpr int kDefaultPercent = 70;
// World units of extra edge radius per world unit between caster and receiver, at strength 1.
inline constexpr float kMaxSlope = 0.06f;
// The most the edge widens over the map's own edge, in world units, at strength 1.
inline constexpr float kMaxGrowthWorld = 40.0f;

// The strength for the saved settings: SunShadowSoftEdgesStrength / 100 (0 to 1); 0 when not available
// (no HD textures, the browser build), on the Low graphics preset, for SunShadowSoftEdges 0 or an unknown
// value, and at strength 0. strengthPercent is clamped to 0..100.
[[nodiscard]] float Resolve(int sunShadowSoftEdges, int strengthPercent, bool available, bool lowPreset);

// Why it does not show right now (the Options window's SunShadowSoftEdges row).
enum class Block : std::uint8_t
{
    None,
    // HD textures are off in this session (they switch on with the next start).
    NoHdTextures,
    // The Low graphics preset keeps it off.
    LowPreset,
    // It needs Sun shadows (Medium or High).
    NoSunShadows,
};

// The first reason that applies, in the order of Block.
[[nodiscard]] Block ResolveBlock(bool hdTextures, bool lowPreset, bool sunShadows);
// Whether a reason locks the Options row.
[[nodiscard]] bool Locks(Block block);

// The edge's growth per world unit between caster and receiver at a strength (0..1, clamped).
[[nodiscard]] float Slope(float strength);
// The most the edge widens, in world units, at a strength (0..1, clamped).
[[nodiscard]] float GrowthWorld(float strength);
// Gives a frame the widening edge of a strength; leaves it as it is at strength 0 or below.
void ApplyToFrame(mu::SunShadowFrame& frame, float strength);

// Engine side (SunShadowSoftEdgesSetting.cpp).

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] SunShadowSoftEdges settings, at start-up once the graphics preset and HD textures are
// known, and again by the Options window (a change of graphics preset). Logs only when the strength
// changes; the next frame's sun shadow takes it.
void ApplyFromConfig();
// The strength in use (0..1); 0 = off.
[[nodiscard]] float Strength();
// Why it does not show right now, from the session's HD textures, graphics preset and sun shadows.
[[nodiscard]] Block CurrentBlock();
// Saves [Render] SunShadowSoftEdges and SunShadowSoftEdgesStrength (Options > Graphics) and applies
// them at once.
void Choose(bool enabled, int strengthPercent);

} // namespace Render::Lighting::SunShadowSoftEdges
