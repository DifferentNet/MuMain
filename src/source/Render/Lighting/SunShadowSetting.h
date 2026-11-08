#pragma once

#include "Data/GameData/MapData/LightMood.h"
#include "Render/Renderer/SunShadow.h"

// The sun shadows of the world (desktop, Enhanced lighting): the Options level and the frame's
// shadow box. Each map's sun comes from its light mood (sunAzimuth, sunElevation, sunShadow,
// sunSoftness in Data/Maps/LightMood.json). While they are on, the flat Classic shadows are off.
namespace Render::Lighting::SunShadow
{

inline constexpr int LevelOff = 0;
inline constexpr int LevelMedium = 1; // 2048 shadow map
inline constexpr int LevelHigh = 2;   // 4096 shadow map

// From the next frame on; out-of-range values clamp. Nothing is saved.
void SetLevel(int level);
[[nodiscard]] int Level();

// The shadow box of a frame around `focus` (world units) with a map's mood at a level.
[[nodiscard]] mu::SunShadowFrame BuildFrame(const Data::Maps::LightMood& mood, int level, const float focus[3]);

// Hands the renderer the frame's sun shadow around `focus`, from the active world's mood.
void Submit(const float focus[3]);

// The renderer draws this frame's sun shadow: the flat Classic shadows stay off.
[[nodiscard]] bool Active();

} // namespace Render::Lighting::SunShadow
