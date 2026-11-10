#include "stdafx.h"

#include "Render/Look/GraphicsPresetSetting.h"

#include "App/Platform/Windows/Winmain.h"
#include "Core/Utilities/PlatformInfo.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/Lighting/SunShadowSetting.h"
#include "Render/Look/LookPresetSetting.h"
#include "Render/Models/ModelSmoothing.h"
#include "Render/PostProcessing/ScreenFilterSetting.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Textures/HdTextureSize.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/NormalMaps.h"

#include <string>

namespace Render::Look
{
namespace
{
constexpr float PercentPerUnit = 100.0f;

void ApplySunShadows()
{
    Render::Lighting::SunShadow::SetLevel(GameConfig::GetInstance().GetSunShadows());
}

void ApplyScreenFilters()
{
    const GameConfig& config = GameConfig::GetInstance();
    PostProcessing::SetScreenFilters({config.GetFxaa(), static_cast<float>(config.GetSharpen()) / PercentPerUnit});
}

std::string ReportedGpuName()
{
    const char* reportedName = mu::GetRenderer().GetGPUDeviceName();
    return reportedName != nullptr ? reportedName : "";
}

// Remaster on a weak GPU with the Low preset: the models load as they are in the files (smoothing splits
// every triangle into four or sixteen) and without normal maps, which only the Enhanced lighting reads and
// which add a quarter to the HD textures' memory. For this run only, nothing is saved: with another preset
// the next start loads both again. Called before the first model or texture loads.
void LightenModelsForWeakGpu(const std::string& gpuName)
{
    if (!IsWeakGpu(gpuName) || GameConfig::GetInstance().GetGraphicsPreset() != GraphicsPreset::Low)
    {
        return;
    }
    if (Render::Models::Smoothing::Level() == 0 && !Render::Textures::NormalMaps::Enabled())
    {
        // Classic client, or HD textures off: there is nothing to lighten.
        return;
    }
    Render::Models::Smoothing::SetLevel(0);
    Render::Textures::NormalMaps::SetEnabled(false);
    mu::log::Get("render")->info("Weak GPU '{}' on Low: model smoothing and normal maps off for this run", gpuName);
}

// Remaster: whether the HD textures load at half size this run ([Render] HdTextureSize; Auto goes by
// the GPU and the system memory, Render/Textures/HdTextureSize.h). A GPU whose name tells nothing counts
// as integrated, as for the Auto preset. Called before the first texture loads.
void ChooseHdTextureSize(const std::string& gpuName)
{
    namespace HdSize = Render::Textures::HdSize;
    if (!Render::Textures::Hd::Enabled())
    {
        HdSize::SetHalf(false);
        return;
    }
    const HdSize::Setting setting = HdSize::SettingFromConfig(GameConfig::GetInstance().GetHdTextureSize());
    const bool sharedMemoryGpu = ClassifyGpu(gpuName) != GpuClass::Discrete;
    const int systemRamMb = Core::Platform::GetSystemMemoryMB();
    const bool half = HdSize::ChooseHalf(setting, sharedMemoryGpu, systemRamMb);
    HdSize::SetHalf(half);
    mu::log::Get("render")->info("HD textures at {} size ({}; GPU '{}', {} MiB of system memory)",
                                 half ? "half" : "full", setting == HdSize::Setting::Auto ? "auto" : "set by the player",
                                 gpuName, systemRamMb);
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
    // In the browser the presets leave the sun shadows alone (their own Options row, off unless the player turns
    // them on, a phone's GPU can't spare them): the value is taken from the preset row the other settings match,
    // so the shadows never keep a preset from matching.
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
#ifndef __EMSCRIPTEN__
    config.SetSunShadows(settings.sunShadows);
#endif
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

bool RemasterGraphicsOptions()
{
#ifdef __EMSCRIPTEN__
    return true;
#else
    return Render::Textures::Hd::Installed();
#endif
}

void ApplyStartupGraphics()
{
    GameConfig& config = GameConfig::GetInstance();
    if (!RemasterGraphicsOptions())
    {
        // Classic client: OpenMU's settings only, so our filters, sun shadows and HD textures stay off
        // even where an older client saved them on.
        config.SetFxaa(false);
        config.SetSharpen(0);
        config.SetSunShadows(0);
        config.SetHdTextures(false);
    }
    // Auto picks a preset only on a new install (no [Render] Look yet) and only on the desktop: an existing
    // player's choices stay as they are, and the browser's WebGPU device reports no GPU name to go by.
#ifdef __EMSCRIPTEN__
    const bool chooseAuto = false;
#else
    const bool chooseAuto =
        RemasterGraphicsOptions() && config.GetGraphicsPreset() == GraphicsPreset::Auto && !config.HadSavedLook();
#endif
    if (chooseAuto)
    {
        const std::string gpuName = ReportedGpuName();
        const GraphicsPreset chosen = ChooseAutoGraphicsPreset(gpuName);
        mu::log::Get("render")->info("Graphics preset Auto: {} for GPU '{}'", PresetName(chosen), gpuName);
        ApplyGraphicsPreset(chosen);
        LightenModelsForWeakGpu(gpuName);
        ChooseHdTextureSize(gpuName);
        return;
    }
    ApplyLookPreset(config.GetLook());
    ApplyScreenFilters();
    ApplySunShadows();
    if (config.GetGraphicsPreset() == GraphicsPreset::Auto)
    {
        // Name the preset the kept settings match (Custom if none), so Auto doesn't wait for the next start.
        RefreshGraphicsPreset();
    }
#ifndef __EMSCRIPTEN__
    const std::string gpuName = ReportedGpuName();
    LightenModelsForWeakGpu(gpuName);
    ChooseHdTextureSize(gpuName);
#endif
}

} // namespace Render::Look
