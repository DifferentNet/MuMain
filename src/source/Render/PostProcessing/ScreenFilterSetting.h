#pragma once

// Screen filters: FXAA and sharpening of the finished 3D scene, before the
// interface is drawn. They are the player's settings ([Render] Fxaa and
// Sharpen, chosen by the graphics preset or one by one) and work in both
// looks: switching either on turns the post-processing pass on even where the
// look's bloom and colour grading are off (the frame then gets only the
// filters). The interface is never filtered.
namespace Render::PostProcessing
{
struct ScreenFilters
{
    // Fast approximate anti-aliasing: smooths edges in one cheap pass. Meant
    // for when multisampling is off (weak GPUs); with it on it adds little.
    bool fxaa = false;
    // Contrast-adaptive sharpening strength, 0 (off) to 1.
    float sharpen = 0.0f;

    bool operator==(const ScreenFilters&) const = default;
};

inline constexpr float MaxSharpen = 1.0f;

// Applies the filters from the next frame on (clamping the sharpening to
// 0..MaxSharpen). Call after the look's features were applied or at any time
// after; the post-processing pass stays on while the look asks for it or any
// filter is on.
void SetScreenFilters(const ScreenFilters& filters);
[[nodiscard]] ScreenFilters GetScreenFilters();
// True while either filter is on.
[[nodiscard]] bool ScreenFiltersOn();
} // namespace Render::PostProcessing
