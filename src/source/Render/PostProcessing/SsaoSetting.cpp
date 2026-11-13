#include "stdafx.h"

#include "Render/PostProcessing/SsaoSetting.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Render/Lighting/EnhancedLightingSetting.h"
#include "Render/PostProcessing/PostProcessingSetting.h"
#include "Render/Renderer/SdlGpuSsao.h"
#include "Render/Textures/HdTextures.h"

#include <optional>

namespace Render::PostProcessing::Ssao
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

// The strength last given to the renderer, for logging only its changes.
std::optional<float> s_appliedStrength;
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    const float strength = Resolve(config.GetSsao(), config.GetSsaoStrength(),
                                   Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
    mu::SetSsaoSettings(strength);
    if (s_appliedStrength == strength)
    {
        return;
    }
    s_appliedStrength = strength;
    mu::log::Get("render")->info("SSAO {} (strength {} %)", strength > 0.0f ? "on" : "off",
                                 static_cast<int>(strength * kPercentPerUnit));
}

Block CurrentBlock()
{
    const bool enhancedActive =
        Render::Lighting::EnhancedLightingActive() && Render::PostProcessing::PostProcessingActive();
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen(), enhancedActive,
                        mu::HasSsaoPipelines());
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetSsao(enabled ? CfgDefaults::CfgMaxSsao : CfgDefaults::CfgMinSsao);
    config.SetSsaoStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

} // namespace Render::PostProcessing::Ssao
