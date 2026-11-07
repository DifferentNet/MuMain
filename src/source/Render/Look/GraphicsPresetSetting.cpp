#include "stdafx.h"

#include "Render/Look/GraphicsPresetSetting.h"

#include "App/Platform/Windows/Winmain.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/Look/LookPresetSetting.h"
#include "Render/PostProcessing/ScreenFilterSetting.h"
#include "Render/Renderer/MuRenderer.h"

#include <string>

namespace Render::Look
{
namespace
{
constexpr float PercentPerUnit = 100.0f;

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
}

} // namespace Render::Look
