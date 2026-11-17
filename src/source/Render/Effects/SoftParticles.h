// SoftParticles.h: soft effect sprites of the Remaster (Render/Effects/SkillFx.h, Feature::SoftParticles).
//
// An effect sprite (RenderSprite) carries its softness, a world distance, in the Vertex3D normal's x.
// Where the frame already has its world depth (SSAO or haze on), the renderer draws such a sprite
// through a soft twin (soft_particles.vert + basic_textured_fog_soft[_glow].frag), which fades it by
// Fade() where it cuts into the ground, walls and models: an alpha-blended sprite in its alpha, an
// additive or glow one in its colour. Without that depth, or with the feature off, the sprite draws as
// before (the twins never read the normal). This header is the CPU mirror the tests check; the shaders
// (soft_particles.hlsli) are the implementation.
#pragma once

#include <algorithm>

#include "Render/Renderer/MuRenderer.h"

namespace Render::Effects::SoftParticles
{

// The softness is this share of the sprite's smaller half size, within [kMinSoftness, kMaxSoftness].
inline constexpr float kSoftnessScale = 0.35f;
inline constexpr float kMinSoftness = 8.0f;
inline constexpr float kMaxSoftness = 40.0f;

// The world distance over which a sprite of half sizes halfWidth x halfHeight fades in front of the
// geometry behind it.
[[nodiscard]] constexpr float Softness(const float halfWidth, const float halfHeight)
{
    return std::clamp(kSoftnessScale * (std::min)(halfWidth, halfHeight), kMinSoftness, kMaxSoftness);
}

// The share of a sprite pixel kept: 0 where it touches the geometry behind it, 1 from `softness` in
// front of it on. Nothing behind (sceneDistance < 0: no depth) or no softness keeps it whole.
[[nodiscard]] constexpr float Fade(const float sceneDistance, const float particleDistance, const float softness)
{
    if (softness <= 0.0f || sceneDistance < 0.0f)
    {
        return 1.0f;
    }
    return std::clamp((sceneDistance - particleDistance) / softness, 0.0f, 1.0f);
}

// How a soft twin applies the fade.
enum class Twin : std::uint8_t
{
    None,  // the draw has no soft twin
    Alpha, // multiplies the alpha (Alpha blending: the sprite turns clear)
    Glow,  // multiplies the colour (Additive and Glow blending: the sprite adds less light)
};

// The twin of a recorded draw: blended (Alpha, Additive or Glow), depth-tested, not depth-writing.
[[nodiscard]] constexpr Twin TwinFor(const bool blendEnabled, const mu::BlendMode mode, const bool depthTest,
                                     const bool depthWrite)
{
    if (!blendEnabled || !depthTest || depthWrite)
    {
        return Twin::None;
    }
    switch (mode)
    {
    case mu::BlendMode::Alpha:
        return Twin::Alpha;
    case mu::BlendMode::Additive:
    case mu::BlendMode::Glow:
        return Twin::Glow;
    default:
        return Twin::None;
    }
}

} // namespace Render::Effects::SoftParticles
