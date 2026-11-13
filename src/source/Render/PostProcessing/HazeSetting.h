// HazeSetting.h: distance haze and low ground fog in the Remaster (desktop, HD textures on), from
// the player's [Render] Haze settings and each map's light mood (Data/Maps/LightMood.json: haze,
// hazeR/G/B, groundFog, groundFogHeight, hazeSun).
//
//   After the opaque world is drawn (and darkened by SSAO), one pass over the world's depth (the
//   same depth SSAO draws, SdlGpuHaze) lays a coloured veil over it. It starts kClearMargin behind
//   the hero, so the hero and the characters near him are never touched, grows smoothly towards
//   the start of the old Classic fog and fades out under that fog, which keeps hiding the far
//   edge as before. The ground fog thickens low places (below the hero's ground level plus the
//   map's layer height) further away. Effects, particles and the UI are drawn after it and never
//   get it; the sky behind the world (nothing drawn) gets none either.
//
//   The amounts, for a pixel at view distance L and world height z:
//     d        = saturate((L - start) / (end - start)), start = hero distance + kClearMargin
//                (no hero: kNoHeroStartShare of the fog start), end = the fog start
//     distance = haze * d^2 (3 - 2d)                      (smooth, 0 at the start)
//     ground   = groundFog * h^2 * sqrt(d), h = saturate(1 - (z - reference) / layer height)
//     amount   = min((1 - (1 - distance)(1 - ground)) * fade-out under the fog, kMaxAmount)
//   haze and groundFog are already multiplied by the strength (HazeStrength / kDefaultPercent).
//   post_haze.frag computes the same.
#pragma once

#include "Render/PostProcessing/SsaoSetting.h"

#include <algorithm>
#include <cmath>

namespace Render::PostProcessing::Haze
{

inline constexpr float kPercentPerUnit = 100.0f;
// [Render] HazeStrength at which the maps' haze shows as their light mood gives it; 100 is
// 100 / 70 of it (the cap kMaxAmount always holds).
inline constexpr int kDefaultPercent = 70;
// World units behind the hero (along the view) where the haze starts: he and those near him stay clear.
inline constexpr float kClearMargin = 300.0f;
// Without a hero, the haze starts at this share of the fog start.
inline constexpr float kNoHeroStartShare = 0.55f;
// A shorter haze range (zoomed far out, a cutscene camera) draws none that frame.
inline constexpr float kMinRange = 200.0f;
// The most a pixel ever takes of the haze colour.
inline constexpr float kMaxAmount = 0.45f;
// The ground fog's reference height follows the hero's with this time constant (seconds)...
inline constexpr float kReferenceSeconds = 1.0f;
// ...and jumps to it on a change of map or a move farther than this (a teleport).
inline constexpr float kReferenceSnap = 500.0f;

// The strength for the saved settings: HazeStrength / kDefaultPercent (0 to 100 / 70); 0 when not
// available (no HD textures, the browser build), on the Low graphics preset, for Haze 0 or an
// unknown value, and at strength 0. strengthPercent is clamped to 0..100.
[[nodiscard]] inline float Resolve(int haze, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || haze != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, 100)) / static_cast<float>(kDefaultPercent);
}

// The view distances the haze grows over; none when the range is too short.
struct Range
{
    float start = 0.0f;
    float end = 0.0f;
    bool valid = false;
};

// From the hero's view distance (when he is in view) to the start of the fog fade.
[[nodiscard]] inline Range ResolveRange(bool heroVisible, float heroDistance, float fogStart)
{
    Range range;
    range.start = heroVisible ? heroDistance + kClearMargin : fogStart * kNoHeroStartShare;
    range.end = fogStart;
    range.valid = range.end - range.start >= kMinRange;
    return range;
}

// How far a view distance lies into the range, 0 to 1.
[[nodiscard]] inline float RangeShare(float viewDistance, const Range& range)
{
    if (!range.valid)
    {
        return 0.0f;
    }
    return std::clamp((viewDistance - range.start) / (range.end - range.start), 0.0f, 1.0f);
}

// The distance haze at a share of the range (smoothstep).
[[nodiscard]] inline float DistanceAmount(float share, float haze)
{
    return haze * share * share * (3.0f - 2.0f * share);
}

// The ground fog at world height z, a reference height and layer height, at a share of the range:
// none above the layer, more the lower and the farther.
[[nodiscard]] inline float GroundAmount(float z, float referenceHeight, float layerHeight, float groundFog,
                                        float share)
{
    if (layerHeight <= 0.0f || share <= 0.0f)
    {
        return 0.0f;
    }
    const float low = std::clamp(1.0f - (z - referenceHeight) / layerHeight, 0.0f, 1.0f);
    return groundFog * low * low * std::sqrt(share);
}

// 1 before the fog fade, 0 at its end: the old fog takes over.
[[nodiscard]] inline float FogFadeOut(float viewDistance, float fadeStart, float fadeEnd)
{
    if (fadeEnd <= fadeStart)
    {
        return viewDistance < fadeStart ? 1.0f : 0.0f;
    }
    return 1.0f - std::clamp((viewDistance - fadeStart) / (fadeEnd - fadeStart), 0.0f, 1.0f);
}

// Both veils over one another, faded out under the fog, never above kMaxAmount.
[[nodiscard]] inline float Combine(float distanceAmount, float groundAmount, float fogFadeOut)
{
    const float both = 1.0f - (1.0f - distanceAmount) * (1.0f - groundAmount);
    return std::min(both * fogFadeOut, kMaxAmount);
}

// The ground fog's reference height after `seconds` (exponential, kReferenceSeconds), or the
// target itself when `snap` (a new map) or after a jump farther than kReferenceSnap.
[[nodiscard]] inline float SmoothReference(float previous, float target, float seconds, bool snap)
{
    if (snap || std::abs(target - previous) > kReferenceSnap || seconds < 0.0f)
    {
        return target;
    }
    const float keep = std::exp(-seconds / kReferenceSeconds);
    return target + (previous - target) * keep;
}

// Why it does not show right now (the Options window's Haze row): as SSAO's, which draws the
// depth it is laid over.
using Block = Render::PostProcessing::Ssao::Block;
using Render::PostProcessing::Ssao::Locks;
using Render::PostProcessing::Ssao::ResolveBlock;

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] Haze settings, at start-up once the graphics preset and HD textures are known, and
// again by the Options window (a change of graphics preset). Logs only when the strength changes.
void ApplyFromConfig();
// Why it does not show right now, from the session's HD textures, graphics preset, Enhanced
// features and renderer.
[[nodiscard]] Block CurrentBlock();
// Saves [Render] Haze and HazeStrength (Options > Graphics) and applies them at once.
void Choose(bool enabled, int strengthPercent);
// The current map's haze for this frame (its light mood, the hero), given to the renderer before
// the end of the opaque world.
void PushFrame();

} // namespace Render::PostProcessing::Haze
