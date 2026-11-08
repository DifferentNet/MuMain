#include "stdafx.h"

#include "Render/Lighting/SunShadowSetting.h"

#include "Render/Lighting/LightMoodSetting.h"
#include "Render/Renderer/MuRenderer.h"

#include <algorithm>
#include <cmath>

namespace Render::Lighting::SunShadow
{
namespace
{
int s_level = LevelOff;

constexpr double DegreesToRadians = 3.14159265358979323846 / 180.0;
// Half the side of the shadow box: about 20 tiles each way, what the game camera shows.
constexpr float HalfExtent = 2000.0f;
// Depth of the box along the sun: tall objects and long shadows of a low sun.
constexpr float DepthRange = 6000.0f;
} // namespace

void SetLevel(int level)
{
    s_level = std::clamp(level, LevelOff, LevelHigh);
}

int Level()
{
    return s_level;
}

mu::SunShadowFrame BuildFrame(const Data::Maps::LightMood& mood, int level, const float focus[3])
{
    mu::SunShadowFrame frame{};
    frame.mapSize = mu::SunShadowMapSize(level);
    frame.enabled = frame.mapSize > 0u && mood.sunShadow > 0.0;
    const double azimuth = mood.sunAzimuth * DegreesToRadians;
    const double elevation = std::clamp(mood.sunElevation, 20.0, 85.0) * DegreesToRadians;
    frame.towardSun = {static_cast<float>(std::cos(elevation) * std::cos(azimuth)),
                       static_cast<float>(std::cos(elevation) * std::sin(azimuth)),
                       static_cast<float>(std::sin(elevation))};
    frame.focus = {focus[0], focus[1], focus[2]};
    frame.halfExtent = HalfExtent;
    frame.depthRange = DepthRange;
    frame.strength = static_cast<float>(std::clamp(mood.sunShadow, 0.0, 1.0));
    frame.softness = static_cast<float>(std::clamp(mood.sunSoftness, 2.0, 40.0));
    return frame;
}

void Submit(const float focus[3])
{
    mu::GetRenderer().SetSunShadow(BuildFrame(LightMood::WorldMood(), s_level, focus));
}

bool Active()
{
    return mu::GetRenderer().IsSunShadowActive();
}

} // namespace Render::Lighting::SunShadow
