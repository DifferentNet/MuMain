#pragma once

#include "Data/GameConfig/GameConfigValidation.h"

#include <string_view>

// The graphics presets (Low, Medium, High, Ultra): each one a row of the settings below. A player's
// settings that match no row are Custom. The presets set the look, the Enhanced features, the
// anti-aliasing, the screen filters and the anisotropy; they never touch the effect and shadow
// switches, VSync, the FPS cap or the window size.
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

// The row of a concrete preset (Low to Ultra); Low's row for Auto and Custom, which have none.
[[nodiscard]] GraphicsSettings GraphicsPresetSettings(GraphicsPreset preset);
// The preset whose row equals `settings`, else Custom.
[[nodiscard]] GraphicsPreset MatchGraphicsPreset(const GraphicsSettings& settings);
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
