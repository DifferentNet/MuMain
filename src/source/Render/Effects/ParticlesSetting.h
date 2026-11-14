// ParticlesSetting.h: ambient air particles in the Remaster (desktop, HD textures on), from the
// player's [Render] Particles settings and each map's light mood (Data/Maps/LightMood.json:
// particles, particlesDensity, particles2, particles2Density, particlesR/G/B).
//
//   Up to kMaxLayers layers per map, each one kind (Kind: dust, pollen, fireflies, fine snow, ash,
//   embers) drawn by the GPU alone (SdlGpuParticles, ambient_particles.vert): no textures, no
//   buffers, no work per particle on the CPU. Particle n of a layer sits at a hashed point of a
//   box kBoxSizeXY wide around the hero, drifts with the wind, falls or rises, sways and flickers,
//   all in whole turns over kPeriodSeconds so the period's wrap never shows; its place wraps
//   around the box (WrapShare), so it stays put in the world while the hero walks, and fades out
//   at the box's sides, top and bottom and near the camera. A particle is never smaller than its
//   kind's smallest pixel size (fainter instead) nor larger than its largest one.
//   Drawn right after the opaque world (and SSAO and the haze) and before joints, effects and the
//   UI: a skill effect always covers a particle, never the reverse. No bloom, no sun shadow.
//   Maps without the keys, Particles 0, strength 0, the Low preset and no HD textures draw none
//   and the frame is the same as without this feature.
//
//   The kind in LightMood.json is its number (Kind): 1 dust, 2 pollen, 3 fireflies, 4 fine snow,
//   5 ash, 6 embers; 0 or anything else is none.
#pragma once

#include "Render/Models/WindSway.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Render::Effects::Particles
{

enum class Kind : std::uint8_t
{
    None,
    Dust,
    Pollen,
    Fireflies,
    Snow,
    Ash,
    Embers,
};
inline constexpr int kKindCount = 7;

// [Render] ParticlesStrength at which a layer has its table count (times the map's density); 100
// is 100 / 70 of it. The slider changes the density only, never the brightness.
inline constexpr int kDefaultPercent = 70;
// The most particles a layer ever draws.
inline constexpr int kMaxCount = 1024;
// Every motion makes whole turns over this period (seconds).
inline constexpr double kPeriodSeconds = 2048.0;
// The box around the hero, in world units (a tile is 100); its height is the kind's.
inline constexpr float kBoxSizeXY = 1800.0f;
// Particles fade out from this share of the box's half size to its side.
inline constexpr float kEdgeFadeStart = 0.75f;
// Near the camera they fade in from kNearFadeStart over kNearFadeLength (view distance).
inline constexpr float kNearFadeStart = 300.0f;
inline constexpr float kNearFadeLength = 300.0f;
// The share of the drift kept with the wind off: the air still moves a little.
inline constexpr float kStillAirShare = 0.35f;
// Entering or leaving a place where they show, they fade with this time constant (seconds).
inline constexpr float kPresenceSeconds = 0.6f;
// The pixel sizes of the kinds are for this viewport height.
inline constexpr float kReferencePixelHeight = 1080.0f;
// Layers per map.
inline constexpr int kMaxLayers = 2;

// How a kind looks and moves. Speeds in world units per second; the box's height runs from the
// reference height + zMin over zSize.
struct Look
{
    int count;
    float minRadius, maxRadius; // world units
    float minPixels, maxPixels; // radius on screen at kReferencePixelHeight
    float alpha;                // the peak opacity
    float color[3];
    bool additive, glow;
    float zMin, zSize;
    float drift;        // along the wind
    float fall;         // along z (negative falls, positive rises)
    float speedSpread;  // 0..1: how much slower some particles are
    float swayXY, swayZ; // world units
    float swayHz;
    float flickerDepth, flickerHz;
    float riseFade; // 0..1: how much they fade towards the top of the box
};

inline constexpr std::array<Look, kKindCount> kLooks{{
    // None
    {0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, false, false, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
     0.0f, 0.0f, 0.0f, 0.0f},
    // Dust
    {420, 1.2f, 2.6f, 1.5f, 4.5f, 0.30f, {1.00f, 0.96f, 0.86f}, false, false, -20.0f, 480.0f, 9.0f, -2.0f, 0.5f,
     10.0f, 6.0f, 0.12f, 0.5f, 0.25f, 0.0f},
    // Pollen
    {160, 1.5f, 3.0f, 1.5f, 4.5f, 0.38f, {1.00f, 0.95f, 0.60f}, false, false, -20.0f, 420.0f, 14.0f, -6.0f, 0.5f,
     16.0f, 8.0f, 0.15f, 0.3f, 0.2f, 0.0f},
    // Fireflies
    {28, 6.0f, 8.0f, 3.0f, 14.0f, 0.90f, {0.75f, 1.00f, 0.35f}, true, true, 10.0f, 220.0f, 0.0f, 0.0f, 0.0f, 55.0f,
     25.0f, 0.08f, 1.0f, 0.3f, 0.0f},
    // Snow (fine)
    {420, 1.0f, 2.4f, 1.5f, 4.5f, 0.55f, {0.95f, 0.97f, 1.00f}, false, false, -20.0f, 520.0f, 35.0f, -90.0f, 0.4f,
     10.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f},
    // Ash
    {260, 1.2f, 2.8f, 1.5f, 4.5f, 0.40f, {0.62f, 0.60f, 0.58f}, false, false, -20.0f, 520.0f, 10.0f, -22.0f, 0.5f,
     18.0f, 6.0f, 0.2f, 0.0f, 0.0f, 0.0f},
    // Embers
    {50, 2.0f, 3.5f, 2.0f, 6.0f, 0.85f, {1.00f, 0.45f, 0.12f}, true, true, -20.0f, 420.0f, 8.0f, 30.0f, 0.5f, 20.0f,
     8.0f, 0.25f, 0.5f, 1.5f, 0.8f},
}};

[[nodiscard]] inline const Look& LookOf(Kind kind)
{
    const auto index = static_cast<std::size_t>(kind);
    return kLooks[index < kLooks.size() ? index : 0u];
}

// The kind for a light mood value: rounded; outside 1..6 or not finite is none.
[[nodiscard]] inline Kind KindFromValue(double value)
{
    if (!std::isfinite(value))
    {
        return Kind::None;
    }
    const double rounded = std::round(value);
    if (rounded < 1.0 || rounded > static_cast<double>(kKindCount - 1))
    {
        return Kind::None;
    }
    return static_cast<Kind>(static_cast<int>(rounded));
}

// The strength for the saved settings: ParticlesStrength / kDefaultPercent (0 to 100 / 70); 0 when
// not available (no HD textures, the browser build), on the Low graphics preset, for Particles 0 or
// an unknown value, and at strength 0. strengthPercent is clamped to 0..100.
[[nodiscard]] inline float Resolve(int particles, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || particles != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, 100)) / static_cast<float>(kDefaultPercent);
}

// How many particles a layer of `kind` draws at the map's density and the strength: rounded,
// 0 to kMaxCount.
[[nodiscard]] inline int Count(Kind kind, float mapDensity, float strength)
{
    const float wanted = static_cast<float>(LookOf(kind).count) * mapDensity * strength;
    if (!(wanted > 0.0f))
    {
        return 0;
    }
    return static_cast<int>(std::min(std::round(wanted), static_cast<float>(kMaxCount)));
}

// The frame's phase over kPeriodSeconds, 0 (inclusive) to 1 (exclusive), never negative.
[[nodiscard]] inline float Phase(double worldTimeMs)
{
    double share = std::fmod(worldTimeMs / 1000.0, kPeriodSeconds) / kPeriodSeconds;
    if (share < 0.0)
    {
        share += 1.0;
    }
    const auto phase = static_cast<float>(share);
    return phase >= 1.0f || !(phase >= 0.0f) ? 0.0f : phase;
}

// Box lengths travelled over the period at this speed (the shader rounds it to whole turns).
[[nodiscard]] inline float Turns(float unitsPerSecond, float boxLength)
{
    if (boxLength <= 0.0f)
    {
        return 0.0f;
    }
    return unitsPerSecond * static_cast<float>(kPeriodSeconds) / boxLength;
}

// A place in the box as a share of its size, wrapped to -0.5 (inclusive) to 0.5 (exclusive): the
// shader's frac(share + 0.5) - 0.5.
[[nodiscard]] inline float WrapShare(float share)
{
    const float shifted = share + 0.5f;
    float fraction = shifted - std::floor(shifted);
    if (fraction >= 1.0f)
    {
        fraction = 0.0f;
    }
    return fraction - 0.5f;
}

[[nodiscard]] inline float SmoothStep(float edge0, float edge1, float x)
{
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// The fade at the box's sides for a place (shares, -0.5..0.5): 1 inside kEdgeFadeStart of the half
// size, 0 at the side.
[[nodiscard]] inline float EdgeFade(float relX, float relY)
{
    const float edge = std::max(std::abs(relX), std::abs(relY)) * 2.0f;
    return 1.0f - SmoothStep(kEdgeFadeStart, 1.0f, edge);
}

// The fade near the camera: 0 at kNearFadeStart, 1 from kNearFadeStart + kNearFadeLength.
[[nodiscard]] inline float NearFade(float viewDistance)
{
    return std::clamp((viewDistance - kNearFadeStart) / kNearFadeLength, 0.0f, 1.0f);
}

// A radius on screen held between the kind's pixel limits: below the smallest the particle keeps
// that size and fades by (pixels / smallest)^2; above the largest it is capped.
struct PixelSize
{
    float radius;
    float alphaScale;
};
[[nodiscard]] inline PixelSize ClampPixels(float pixels, float minPixels, float maxPixels)
{
    if (pixels < minPixels)
    {
        const float share = minPixels > 0.0f ? std::max(pixels, 0.0f) / minPixels : 0.0f;
        return {minPixels, share * share};
    }
    if (pixels > maxPixels)
    {
        return {maxPixels, 1.0f};
    }
    return {pixels, 1.0f};
}

// Whether the hero stands where the particles show: the Classic outdoor gates of Lorencia (not on
// tile 4) and Devias (not on tile 3 nor 10 and above); everywhere on other maps.
[[nodiscard]] inline bool Outdoors(int world, int heroTile)
{
    switch (world)
    {
    case 0:
        return heroTile != 4;
    case 2:
        return heroTile != 3 && heroTile < 10;
    default:
        return true;
    }
}

// The presence (0..1) after `seconds` (exponential, kPresenceSeconds), or the target itself when
// `snap` (a new map) or for a negative time.
[[nodiscard]] inline float SmoothPresence(float previous, float target, float seconds, bool snap)
{
    if (snap || seconds < 0.0f)
    {
        return target;
    }
    const float keep = std::exp(-seconds / kPresenceSeconds);
    return target + (previous - target) * keep;
}

// The share of a kind's drift at the wind's strength (0..1): kStillAirShare with the wind off.
[[nodiscard]] inline float WindShare(float windStrength)
{
    return kStillAirShare + (1.0f - kStillAirShare) * std::clamp(windStrength, 0.0f, 1.0f);
}

// Why it does not show right now (the Options window's Particles row): as the wind's.
using Block = Render::Models::Wind::Block;
using Render::Models::Wind::Locks;
using Render::Models::Wind::ResolveBlock;

// Engine side (ParticlesSetting.cpp).

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] Particles settings, at start-up once the graphics preset and HD textures are known,
// and again by the Options window (a change of graphics preset). Logs only when the strength changes.
void ApplyFromConfig();
// Why it does not show right now, from the session's HD textures and graphics preset.
[[nodiscard]] Block CurrentBlock();
// Saves [Render] Particles and ParticlesStrength (Options > Graphics) and applies them at once.
void Choose(bool enabled, int strengthPercent);
// The current map's particle layers for this frame (its light mood, the hero), given to the
// renderer right after the end of the opaque world. Records nothing when there are none.
void PushFrame();

} // namespace Render::Effects::Particles
