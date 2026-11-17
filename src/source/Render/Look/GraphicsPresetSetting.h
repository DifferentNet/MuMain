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
// Start-up, once the renderer exists: on a new desktop install Auto becomes the preset that suits the GPU
// (always Low in the Classic client) and is applied and saved by name; any other preset leaves the saved
// values as they are. Either way the look and the screen filters are applied.
void ApplyStartupGraphics();

// True in the Remaster client and in the browser, false in the Classic client (no Data\HD). It gates the HD
// textures and the Remaster's own effects (SSAO, wind, haze, particles, torch lights, foliage smoothing, soft
// sun shadow edges, HD texture size), which stay off in Classic and Options doesn't show there. The graphics
// preset, FXAA, sharpening and sun shadows are in both clients; Classic's preset rows sharpen less
// (GraphicsPresetSettings).
[[nodiscard]] bool RemasterGraphicsOptions();

} // namespace Render::Look
