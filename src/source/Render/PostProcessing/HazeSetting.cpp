#include "stdafx.h"

#include "Render/PostProcessing/HazeSetting.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Lighting/EnhancedLightingSetting.h"
#include "Render/Lighting/LightMoodSetting.h"
#include "Render/PostProcessing/PostProcessingSetting.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/SdlGpuHaze.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "World/MapInfra/MapManager.h"

#include <optional>

namespace Render::PostProcessing::Haze
{

bool Available()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    return true;
#endif
}

namespace
{
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
constexpr double kMillisecondsPerSecond = 1000.0;

[[nodiscard]] bool LowPresetChosen()
{
    return GameConfig::GetInstance().GetGraphicsPreset() == GraphicsPreset::Low;
}

// The strength last given to the renderer, for logging only its changes.
std::optional<float> s_appliedStrength;

// The ground fog's smoothed reference height, the map and the time it was last worked out for.
struct Reference
{
    int world = -1;
    float height = 0.0f;
    double time = 0.0;
};
Reference s_reference;

// The hero's ground height, smoothed (SmoothReference); snaps on a new map.
float FollowHeroHeight(float heroHeight)
{
    const bool newMap = s_reference.world != gMapManager.WorldActive;
    const float seconds = static_cast<float>((WorldTime - s_reference.time) / kMillisecondsPerSecond);
    s_reference.height = SmoothReference(s_reference.height, heroHeight, seconds, newMap);
    s_reference.world = gMapManager.WorldActive;
    s_reference.time = WorldTime;
    return s_reference.height;
}

void FillColor(const Data::Maps::LightMood& mood, mu::HazeFrame& frame)
{
    // A dim mood's background darkens the haze as well, so it never glows over a dark map.
    const float background = Render::Lighting::LightMood::Active().background;
    frame.color[0] = static_cast<float>(mood.hazeR) * background;
    frame.color[1] = static_cast<float>(mood.hazeG) * background;
    frame.color[2] = static_cast<float>(mood.hazeB) * background;
}

void FillHero(mu::HazeFrame& frame)
{
    if (!Hero)
    {
        return;
    }
    const OBJECT& hero = Hero->Object;
    frame.heroVisible = true;
    frame.heroPosition[0] = hero.Position[0];
    frame.heroPosition[1] = hero.Position[1];
    frame.heroPosition[2] = hero.Position[2];
    frame.referenceHeight = FollowHeroHeight(hero.Position[2]);
}
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    const float strength = Resolve(config.GetHaze(), config.GetHazeStrength(),
                                   Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
    mu::SetHazeSettings(strength);
    if (s_appliedStrength == strength)
    {
        return;
    }
    s_appliedStrength = strength;
    mu::log::Get("render")->info("Haze {} (strength {} %)", strength > 0.0f ? "on" : "off",
                                 static_cast<int>(strength * kDefaultPercent));
}

Block CurrentBlock()
{
    const bool enhancedActive =
        Render::Lighting::EnhancedLightingActive() && Render::PostProcessing::PostProcessingActive();
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen(), enhancedActive,
                        mu::HasHazePipelines());
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetHaze(enabled ? CfgDefaults::CfgMaxHaze : CfgDefaults::CfgMinHaze);
    config.SetHazeStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

void PushFrame()
{
    const Data::Maps::LightMood& mood = Render::Lighting::LightMood::WorldMood();
    mu::HazeFrame frame{};
    frame.haze = static_cast<float>(mood.haze);
    frame.groundFog = static_cast<float>(mood.groundFog);
    if (frame.haze > 0.0f || frame.groundFog > 0.0f)
    {
        frame.groundFogHeight = static_cast<float>(mood.groundFogHeight);
        frame.sun = static_cast<float>(mood.hazeSun);
        const double azimuth = mood.sunAzimuth * kDegreesToRadians;
        frame.sunDirection[0] = static_cast<float>(std::cos(azimuth));
        frame.sunDirection[1] = static_cast<float>(std::sin(azimuth));
        FillColor(mood, frame);
        FillHero(frame);
    }
    mu::GetRenderer().SetHazeFrame(frame);
}

} // namespace Render::PostProcessing::Haze
