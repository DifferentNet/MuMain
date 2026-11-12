// SsaoSetting.h: soft contact shadows (screen-space ambient occlusion) in the Remaster (desktop,
// HD textures on), from the player's [Render] Ssao settings (Remaster plan step 4).
//
//   The opaque world's depth is drawn once more on its own (SdlGpuSsao); at half size each pixel
//   looks at a few points of the depth around it and darkens by how much of its hemisphere they
//   close off (creases, corners, the ground under a model). The result is blurred along the depth
//   and multiplies the scene before effects, particles and the UI are drawn, so glows and windows
//   never get it. It fades out towards the far end of the view and never darkens below kFloor.
#pragma once

#include <algorithm>

namespace Render::PostProcessing::Ssao
{

inline constexpr float kPercentPerUnit = 100.0f;

// The look of the occlusion, in world units (a terrain tile is 100) and full-size pixels.
inline constexpr float kRadiusWorld = 60.0f;      // how far around a point the occluders count
inline constexpr float kMaxRadiusPixels = 48.0f;  // close to the camera the radius stops growing on screen
inline constexpr float kBiasWorld = 2.0f;         // ignores a surface's own small bumps
inline constexpr float kIntensity = 1.0f;
// The darkest the occlusion makes a pixel at full strength: a crease never turns black.
inline constexpr float kFloor = 0.35f;
// Without fog the occlusion fades out over this last part of the view's depth.
inline constexpr float kNoFogFadeStart = 0.6f;

// The strength (0..1) for the saved settings: 0 when not available (no HD textures, the browser
// build), on the Low graphics preset, for Ssao 0 or an unknown value, and at strength 0.
// strengthPercent is clamped to 0..100.
[[nodiscard]] inline float Resolve(int ssao, int strengthPercent, bool available, bool lowPreset)
{
    if (!available || lowPreset || ssao != 1)
    {
        return 0.0f;
    }
    return static_cast<float>(std::clamp(strengthPercent, 0, 100)) / kPercentPerUnit;
}

// The distance along the view direction of a depth-buffer value (0 near, 1 far) of a perspective
// projection with depth 0..1 (GLM_FORCE_DEPTH_ZERO_TO_ONE): p22 = proj[2][2], p32 = proj[3][2].
// d = -p22 + p32 / L, so L = p32 / (d + p22). post_ssao*.frag compute the same.
[[nodiscard]] inline float LinearDepth(float depth, float p22, float p32)
{
    return p32 / (depth + p22);
}

// The view distance over which the occlusion fades out to nothing.
struct FadeRange
{
    float start = 0.0f;
    float end = 0.0f;
};

// With fog the occlusion fades out over the fog's own range, never past the far plane; without
// it (or with a broken range) over the last part of the view's depth.
[[nodiscard]] inline FadeRange ResolveFade(bool fogEnabled, float fogStart, float fogEnd, float farDistance)
{
    if (fogEnabled && fogStart >= 0.0f && fogEnd > fogStart && fogStart < farDistance)
    {
        return {fogStart, std::min(fogEnd, farDistance)};
    }
    return {farDistance * kNoFogFadeStart, farDistance};
}

// Whether this build can draw it (desktop only).
[[nodiscard]] bool Available();
// The [Render] Ssao settings, at start-up once the graphics preset and HD textures are known.
void ApplyFromConfig();

} // namespace Render::PostProcessing::Ssao
