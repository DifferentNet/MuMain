#include "stdafx.h"

#include "Render/Lighting/SoftShadowSetting.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/Renderer/SdlGpuSoftShadow.h"

namespace Render::Lighting::SoftShadow
{

bool Available()
{
#ifdef __EMSCRIPTEN__
    return true;
#else
    return false;
#endif
}

void SetEnabled(bool enabled)
{
    const bool on = enabled && Available();
    mu::SetSoftShadowSettings(on ? DefaultStrength : 0.0f, DefaultLevels);
}

void ApplyFromConfig()
{
    SetEnabled(GameConfig::GetInstance().GetSoftShadows());
}

void Choose(bool enabled)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetSoftShadows(enabled);
    config.Save();
    SetEnabled(enabled);
    mu::log::Get("render")->info("Soft shadows {}", enabled ? "on" : "off");
}

void SetTuning(int strengthPercent, int levels)
{
    if (!Available())
    {
        return;
    }
    mu::SetSoftShadowSettings(static_cast<float>(strengthPercent) / 100.0f, levels);
    mu::log::Get("render")->info("Soft shadow strength {} % levels {}", strengthPercent, levels);
}

} // namespace Render::Lighting::SoftShadow
