#pragma once

#include "Render/Look/GraphicsPreset.h"

// The client's graphics preset (Render/Look/GraphicsPreset.h): applying one, keeping it in step with
// the player's own changes, and the screen filters. It goes through the same functions the options
// window's rows use (the look preset, the Enhanced features, anti-aliasing, anisotropy).
namespace Render::Look
{

// The values the player's config.ini holds now, in the shape of a preset's row.
[[nodiscard]] GraphicsSettings CurrentGraphicsSettings();
// Sets every value of a concrete preset (Low to Ultra), applies them and saves config.ini. Nothing
// happens for Auto or Custom.
void ApplyGraphicsPreset(GraphicsPreset preset);
// After the player changed one of the values by hand: saves the preset that matches them, else Custom.
void RefreshGraphicsPreset();
// Saves the screen filters (FXAA, sharpening in percent), applies them and refreshes the preset.
void ChooseScreenFilters(bool fxaa, int sharpenPercent);
// Start-up, once the renderer exists: Auto becomes the preset that suits the GPU and is applied and
// saved by name; any other preset leaves the saved values as they are. Either way the look and the
// screen filters are applied.
void ApplyStartupGraphics();

} // namespace Render::Look
