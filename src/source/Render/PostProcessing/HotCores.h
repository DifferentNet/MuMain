#pragma once

#include <algorithm>

// HotCores.h: the hot cores of bright effect piles (Remaster skill effects, SkillFx::Feature::Cores),
// kept free of the GPU so it can be checked on its own. post_composite_cores.frag.hlsl is the
// implementation; this is its CPU mirror, as BloomComposite.h is for the plain composite.
//
// The composite reads the effects-only image (the bloom source: the additive draws alone, 8-bit). Where
// one of its channels is clipped and the pixel is bright overall, the scene pixel goes toward white:
//   weight = strength * smoothstep(kCoreMeanLow, 1, mean) * smoothstep(kCoreMaxLow, 1, max)
//   scene += (1 - scene) * weight
// before the bloom and the grading. A lerp toward white keeps the hue (the channel order and the
// (max - mid) / (max - min) ratio), so the MU palette stays. A pile of one pure channel never whitens
// (its mean stays at a third): the 8-bit image has lost the overflow.
namespace Render::PostProcessing::HotCores
{

// The mean of the effect's channels where whitening starts (kCoreMeanLow in the shader).
inline constexpr float kCoreMeanLow = 0.45f;
// The brightest channel of the effect where whitening starts (kCoreMaxLow in the shader).
inline constexpr float kCoreMaxLow = 0.85f;

namespace Detail
{
// HLSL smoothstep: 0 at or below `low`, 1 at or above `high`, a smooth step between.
[[nodiscard]] constexpr float SmoothStep(const float low, const float high, const float value)
{
    const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace Detail

// How hot an effect colour is, 0..1, before the strength: bright overall and clipped in a channel.
[[nodiscard]] constexpr float Heat(const float r, const float g, const float b)
{
    const float mean = (r + g + b) / 3.0f;
    const float peak = std::max(std::max(r, g), b);
    return Detail::SmoothStep(kCoreMeanLow, 1.0f, mean) * Detail::SmoothStep(kCoreMaxLow, 1.0f, peak);
}

// The whitening weight of an effect colour at a strength (0..1, SkillFx::CoreStrength).
[[nodiscard]] constexpr float CoreWeight(const float r, const float g, const float b, const float strength)
{
    return std::clamp(strength, 0.0f, 1.0f) * Heat(r, g, b);
}

// A scene channel pushed toward white by `weight`.
[[nodiscard]] constexpr float Apply(const float scene, const float weight)
{
    return scene + (1.0f - scene) * weight;
}

} // namespace Render::PostProcessing::HotCores
