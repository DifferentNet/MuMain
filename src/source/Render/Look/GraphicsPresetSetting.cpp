#include "stdafx.h"

#include "Render/Look/GraphicsPresetSetting.h"

#include "App/Platform/Windows/Winmain.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/Lighting/SunShadowSetting.h"
#include "Render/Look/LookPresetSetting.h"
#include "Render/PostProcessing/ScreenFilterSetting.h"
#include "Render/Renderer/MuRenderer.h"

#include <string>

namespace Render::Look
{
namespace
{
constexpr float PercentPerUnit = 100.0f;

void ApplySunShadows()
{
#ifdef __EMSCRIPTEN__
    Render::Lighting::SunShadow::SetLevel(Render::Lighting::SunShadow::LevelOff);
#else
    Render::Lighting::SunShadow::SetLevel(GameConfig::GetInstance().GetSunShadows());
#endif
}

void ApplyScreenFilters()
{
    const GameConfig& config = GameConfig::GetInstance();
    PostProcessing::SetScreenFilters({config.GetFxaa(), static_cast<float>(config.GetSharpen()) / PercentPerUnit});
}

std::string PresetName(GraphicsPreset preset)
{
    const std::wstring wide = GameConfigValidation::FormatGraphicsPreset(preset);
    return std::string(wide.begin(), wide.end());
}
} // namespace

GraphicsSettings CurrentGraphicsSettings()
{
    const GameConfig& config = GameConfig::GetInstance();
    GraphicsSettings settings;
    settings.look = config.GetLook();
    settings.enhancedLighting = config.GetEnhancedLighting();
    settings.dynamicLights = config.GetDynamicLights();
    settings.lightMood = config.GetLightMood();
    settings.postProcessing = config.GetPostProcessing();
    settings.antialiasing = config.GetAntialiasing();
    settings.fxaa = config.GetFxaa();
    settings.sharpen = config.GetSharpen();
    settings.anisotropy = config.GetAnisotropy();
    settings.sunShadows = config.GetSunShadows();
#ifdef __EMSCRIPTEN__
    // The browser build has no sun shadows and no Options row: whatever the config holds, the value is taken
    // from the preset row the other settings match, so the shadows never keep a preset from matching.
    for (const GraphicsPreset preset : {GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High,
                                        GraphicsPreset::Ultra})
    {
        GraphicsSettings withRowShadows = settings;
        withRowShadows.sunShadows = GraphicsPresetSettings(preset).sunShadows;
        if (withRowShadows == GraphicsPresetSettings(preset))
        {
            return withRowShadows;
        }
    }
#endif
    return settings;
}

void ApplyGraphicsPreset(GraphicsPreset preset)
{
    if (preset == GraphicsPreset::Auto || preset == GraphicsPreset::Custom)
    {
        return;
    }
    const GraphicsSettings settings = GraphicsPresetSettings(preset);
    GameConfig& config = GameConfig::GetInstance();
    MuSetAnisotropyPreference(settings.anisotropy);
    MuSetAntialiasingPreference(settings.antialiasing);
    config.SetFxaa(settings.fxaa);
    config.SetSharpen(settings.sharpen);
    config.SetSunShadows(settings.sunShadows);
    ApplySunShadows();
    // The look and the player's feature choices together, so the features switch once.
    ApplyLookPreset(settings.look, {settings.enhancedLighting, settings.dynamicLights, settings.lightMood,
                                    settings.postProcessing});
    ApplyScreenFilters();
    config.SetGraphicsPreset(preset);
    config.Save();
    mu::log::Get("render")->info("Graphics preset {}", PresetName(preset));
}

void RefreshGraphicsPreset()
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetGraphicsPreset(MatchGraphicsPreset(CurrentGraphicsSettings()));
    config.Save();
}

void ChooseScreenFilters(bool fxaa, int sharpenPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetFxaa(fxaa);
    config.SetSharpen(sharpenPercent);
    ApplyScreenFilters();
    RefreshGraphicsPreset();
}

void ApplyStartupGraphics()
{
    GameConfig& config = GameConfig::GetInstance();
    if (config.GetGraphicsPreset() == GraphicsPreset::Auto)
    {
        const char* reportedName = mu::GetRenderer().GetGPUDeviceName();
        const std::string gpuName = reportedName != nullptr ? reportedName : "";
        const GraphicsPreset chosen = ChooseAutoGraphicsPreset(gpuName);
        mu::log::Get("render")->info("Graphics preset Auto: {} for GPU '{}'", PresetName(chosen), gpuName);
        ApplyGraphicsPreset(chosen);
        return;
    }
    ApplyLookPreset(config.GetLook());
    ApplyScreenFilters();
    ApplySunShadows();
}

} // namespace Render::Look
