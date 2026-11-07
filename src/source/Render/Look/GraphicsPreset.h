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

    bool operator==(const GraphicsSettings&) const = default;
};

// The row of a concrete preset (Low to Ultra); Low's row for Auto and Custom, which have none.
[[nodiscard]] GraphicsSettings GraphicsPresetSettings(GraphicsPreset preset);
// The preset whose row equals `settings`, else Custom.
[[nodiscard]] GraphicsPreset MatchGraphicsPreset(const GraphicsSettings& settings);
// The preset Auto picks for a GPU by its name as the driver reports it: Low for a software renderer,
// Medium for integrated graphics or an unknown name, High for the rest. Never Auto or Custom.
[[nodiscard]] GraphicsPreset ChooseAutoGraphicsPreset(std::string_view gpuName);

} // namespace Render::Look
