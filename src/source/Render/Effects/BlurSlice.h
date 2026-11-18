// BlurSlice.h: the slice of the swing a player's weapon trail samples this frame (Remaster smooth
// trails, Render/Effects/SkillFx.h, Feature::Ribbons).
//
// CreateWeaponBlur lays ten poses over the last PlaySpeed of the animation every frame, and only a
// 25 FPS reference frame keeps them (MoveBlurs). Between reference frames the next frame's ten poses
// replace the last ones, yet they start a whole PlaySpeed back and so overlap the slice kept at the
// reference frame: at high frame rates the trail's head folds over itself. The head slice covers only
// the animation since the kept slice, REFERENCE_FRAMES_SINCE_STEP of PlaySpeed, with as many of the
// ten poses as that share needs. At 25 FPS and below the share is 1: the full slice, as classic.
#pragma once

#include <algorithm>
#include <cmath>

namespace Render::Effects::BlurSlice
{

struct Slice
{
    int samples; // the poses to lay
    float share; // the part of PlaySpeed they span, 0..1
};

// The classic slice: all `full` poses over the whole PlaySpeed.
[[nodiscard]] constexpr Slice FullSlice(const int full)
{
    return {full, 1.0f};
}

// The head slice `since` reference frames after the kept one: share = since (0..1) and
// ceil(full * share) poses, at least 1.
[[nodiscard]] inline Slice HeadSlice(const float since, const int full)
{
    const float share = std::clamp(since, 0.0f, 1.0f);
    const int samples = std::clamp(static_cast<int>(std::ceil(static_cast<float>(full) * share)), 1, std::max(full, 1));
    return {samples, share};
}

} // namespace Render::Effects::BlurSlice
