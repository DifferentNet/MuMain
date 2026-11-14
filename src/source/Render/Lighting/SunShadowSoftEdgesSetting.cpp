#include "stdafx.h"

#include "Render/Lighting/SunShadowSoftEdges.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Render/Lighting/SunShadowSetting.h"
#include "Render/Textures/HdTextures.h"

#include <cmath>
#include <optional>

namespace Render::Lighting::SunShadowSoftEdges
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
[[nodiscard]] bool LowPresetChosen()
{
    return GameConfig::GetInstance().GetGraphicsPreset() == GraphicsPreset::Low;
}

float s_strength = 0.0f;
// The strength last given, for logging only its changes.
std::optional<float> s_appliedStrength;
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    s_strength = Resolve(config.GetSunShadowSoftEdges(), config.GetSunShadowSoftEdgesStrength(),
                         Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
    if (s_appliedStrength == s_strength)
    {
        return;
    }
    s_appliedStrength = s_strength;
    mu::log::Get("render")->info("Soft sun shadow edges {} (strength {} %)", s_strength > 0.0f ? "on" : "off",
                                 static_cast<int>(std::lround(s_strength * 100.0f)));
}

float Strength()
{
    return s_strength;
}

Block CurrentBlock()
{
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen(),
                        Render::Lighting::SunShadow::Level() > Render::Lighting::SunShadow::LevelOff);
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetSunShadowSoftEdges(enabled ? CfgDefaults::CfgMaxSunShadowSoftEdges
                                         : CfgDefaults::CfgMinSunShadowSoftEdges);
    config.SetSunShadowSoftEdgesStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

} // namespace Render::Lighting::SunShadowSoftEdges
