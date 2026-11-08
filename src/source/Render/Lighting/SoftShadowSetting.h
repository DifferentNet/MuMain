#pragma once

// Soft body shadows: the flat shadows under characters and monsters drawn into a mask, blurred and
// laid over the scene (SdlGpuSoftShadow). Only the browser build draws them; the desktop keeps the
// hard shadows or the sun shadows.
namespace Render::Lighting::SoftShadow
{

// How dark the soft shadows are (0..1) and how many blur steps (1 = half size, 2 = quarter size).
inline constexpr float DefaultStrength = 0.65f;
inline constexpr int DefaultLevels = 1;

// Whether this build can draw them (the Options row shows only then).
[[nodiscard]] bool Available();

// Turns them on or off from the next frame on; nothing is saved.
void SetEnabled(bool enabled);
// The [Render] SoftShadows setting, at start-up.
void ApplyFromConfig();
// Saves [Render] SoftShadows and applies it.
void Choose(bool enabled);

// `$softshadow <strength %> <levels>`: tuning until the next change of the setting.
void SetTuning(int strengthPercent, int levels);

} // namespace Render::Lighting::SoftShadow
