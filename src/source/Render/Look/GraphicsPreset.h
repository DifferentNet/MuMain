#pragma once

#include "Data/GameConfig/GameConfigValidation.h"

#include <string_view>

// The graphics presets (Low, Medium, High, Ultra): each one a row of the settings below. A player's
// settings that match no row are Custom. The presets set the look, the Enhanced features, the
// anti-aliasing, the screen filters, the anisotropy and the sun shadows; they never touch the effect
// switches, VSync, the FPS cap or the window size. The Classic client (no Data\HD, `remaster` false
// below) has the same rows, except that a row which sharpens sharpens ClassicPresetSharpen: the
// original textures look over-sharpened at the Remaster's 40 %.
namespace Render::Look
{

// The values a graphics preset sets (a row of the table in GraphicsPresetSettings).
struct GraphicsSettings
{
    LookPreset look = LookPreset::Classic;
    bool enhancedLighting = false;
    bool dynamicLights = false;
    bool lightMood = false;
    bool postProcessing = false;
    // Multisample count: 0 (off), 2, 4 or 8.
    int antialiasing = 0;
    bool fxaa = false;
    // Sharpening in percent, 0 to 100.
    int sharpen = 0;
    // Anisotropic filtering level; 1 is off.
    int anisotropy = 1;
    // Sun shadows: 0 off, 1 medium, 2 high. The browser build ignores it: there the player alone sets them.
    int sunShadows = 0;

    bool operator==(const GraphicsSettings&) const = default;
};

// The sharpening of the Classic client's Medium, High and Ultra rows, in percent.
inline constexpr int ClassicPresetSharpen = 30;
// The row of a concrete preset (Low to Ultra); Low's row for Auto and Custom, which have none.
// `remaster` false gives the Classic client's row.
[[nodiscard]] GraphicsSettings GraphicsPresetSettings(GraphicsPreset preset, bool remaster = true);
// The preset whose row (the Remaster's, or the Classic client's when `remaster` is false) equals
// `settings`, else Custom.
[[nodiscard]] GraphicsPreset MatchGraphicsPreset(const GraphicsSettings& settings, bool remaster = true);
// Medium, High and Ultra sharpened 30 % until 2026-11-12, now 40 % (the Remaster looked soft). A
// player who kept such a preset has 30 saved: `savedSharpen` becomes the preset's current value.
// Any other preset or value is the player's own and is kept. The Classic client (`remaster` false)
// keeps 30, its own rows' value: `savedSharpen` is returned unchanged.
inline constexpr int LegacyPresetSharpen = 30;
[[nodiscard]] int UpdatedPresetSharpen(GraphicsPreset preset, int savedSharpen, bool remaster = true);
// The Classic client's one-time reset of the settings older Classic clients forced off every start (FXAA,
// sharpening, sun shadows), so a row a Remaster run saved there doesn't come back on. Without the [Render]
// ClassicGraphicsReset marker the three are cleared and `changed` is true (the caller saves the marker either
// way); with it `settings` comes back as it is and `changed` is false.
struct ClassicResetResult
{
    GraphicsSettings settings;
    bool changed = false;
};
[[nodiscard]] ClassicResetResult ResetClassicScreenSettings(GraphicsSettings settings, bool markerPresent);
// What a GPU is, by its name as the driver reports it. An empty or unknown name counts as Integrated.
enum class GpuClass
{
    // No GPU: the CPU draws (Microsoft Basic Render Driver, llvmpipe).
    Software,
    // Integrated graphics too slow for the Enhanced look: Intel HD and UHD Graphics, AMD's pre-Ryzen APUs.
    WeakIntegrated,
    Integrated,
    Discrete,
};
[[nodiscard]] GpuClass ClassifyGpu(std::string_view gpuName);
// Software or WeakIntegrated: the GPUs a new install starts with the lightest settings on.
[[nodiscard]] bool IsWeakGpu(std::string_view gpuName);
// The preset Auto picks for a GPU: Low for a software renderer and for weak integrated graphics, Medium
// for the other integrated graphics or an unknown name, High for the rest. Never Auto or Custom.
[[nodiscard]] GraphicsPreset ChooseAutoGraphicsPreset(std::string_view gpuName);

} // namespace Render::Look
