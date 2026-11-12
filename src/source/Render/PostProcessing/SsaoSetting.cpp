#include "stdafx.h"

#include "Render/PostProcessing/SsaoSetting.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/Renderer/SdlGpuSsao.h"
#include "Render/Textures/HdTextures.h"

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

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    const bool lowPreset = config.GetGraphicsPreset() == GraphicsPreset::Low;
    const float strength = Resolve(config.GetSsao(), config.GetSsaoStrength(),
                                   Available() && Render::Textures::Hd::Enabled(), lowPreset);
    mu::SetSsaoSettings(strength);
    mu::log::Get("render")->info("SSAO {} (strength {} %)", strength > 0.0f ? "on" : "off",
                                 static_cast<int>(strength * kPercentPerUnit));
}

} // namespace Render::PostProcessing::Ssao
