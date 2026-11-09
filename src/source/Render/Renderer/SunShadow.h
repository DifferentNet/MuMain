// SunShadow.h: the sun's shadow map (desktop, Enhanced lighting).
//
// Each frame the scene says where the sun is and which part of the world its shadow box covers
// (IMuRenderer::SetSunShadow). Models drawn inside the frame's shadow scope write their depth as
// the sun sees it into the shadow map; terrain, objects and models in the scope then darken what
// the map says lies in shadow. These are the shared numbers and the uniform block layout.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mu
{

// Shadow map sizes of the Options levels.
inline constexpr std::uint32_t kSunShadowMapMedium = 2048u;
inline constexpr std::uint32_t kSunShadowMapHigh = 4096u;

struct SunShadowFrame
{
    bool enabled = false;
    // World-space direction towards the sun; need not be unit length.
    std::array<float, 3> towardSun{0.45f, -0.35f, 0.82f};
    // World point at the centre of the shadow box (around the hero).
    std::array<float, 3> focus{};
    // Half the width of the square the shadow box covers, in world units.
    float halfExtent = 2000.0f;
    // Depth of the box along the sun direction, centred on the focus.
    float depthRange = 6000.0f;
    // Share of the light a fully shadowed pixel loses (0..1).
    float strength = 0.8f;
    // Soft edge radius in world units (a tile is 100); the uniform turns it into texels.
    float softness = 4.0f;
    std::uint32_t mapSize = kSunShadowMapMedium;
};

// HLSL layout of the block after the scene lighting registers (cbuffer SceneLighting in
// basic_textured_fog_shadow.frag.hlsl and skinned_lit_shadow.frag.hlsl, sun_shadow.hlsli).
struct SunShadowUniform
{
    // World to shadow clip space, column-major (as glm::mat4 and the HLSL default).
    float viewProjection[16]{};
    // x: 1 / map size, y: strength, z: normal offset in world units, w: 1 when on.
    float parameters[4]{};
    // xyz: unit direction towards the sun, w: soft edge radius in texels (0.5 to 32).
    float directionAndSoftness[4]{};
};

static_assert(offsetof(SunShadowUniform, parameters) == 64);
static_assert(offsetof(SunShadowUniform, directionAndSoftness) == 80);
static_assert(sizeof(SunShadowUniform) == 96);

// The sun's view-projection for a frame: orthographic, depth 0..1 from the sun side, with the
// box moved in whole texels so that the shadow edges do not crawl while the hero walks.
[[nodiscard]] std::array<float, 16> SunShadowViewProjection(const SunShadowFrame& frame);

[[nodiscard]] SunShadowUniform PackSunShadowUniform(const SunShadowFrame& frame);

// The level an Options value (0 off, 1 medium, 2 high) stands for: its map size, 0 when off.
[[nodiscard]] std::uint32_t SunShadowMapSize(int level);

} // namespace mu
