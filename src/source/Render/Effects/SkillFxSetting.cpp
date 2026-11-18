#include "stdafx.h"

#include "Render/Effects/SkillFx.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/Look/GraphicsPresetSetting.h"

#include <array>
#include <optional>

namespace Render::Effects::SkillFx
{

namespace
{
std::array<bool, kFeatureCount> s_chosen{};
// The state last logged, for logging only its changes.
std::optional<std::array<bool, kFeatureCount + 1>> s_logged;

constexpr int Index(const Feature feature)
{
    return static_cast<int>(feature);
}

int EffectLevel()
{
    return GameConfig::GetInstance().GetEffectLevel();
}

void Log()
{
    const std::array<bool, kFeatureCount + 1> state{Detail::g_available, s_chosen[0], s_chosen[1], s_chosen[2],
                                                    s_chosen[3], s_chosen[4], s_chosen[5]};
    if (s_logged == state)
    {
        return;
    }
    s_logged = state;
    mu::log::Get("render")->info("Skill effects {}: pools {}, soft {}, flipbook {}, trails {}, decals {}, cores {}",
                                 Detail::g_available ? "available" : "not available", s_chosen[0] ? "on" : "off",
                                 s_chosen[1] ? "on" : "off", s_chosen[2] ? "on" : "off",
                                 s_chosen[3] ? "on" : "off", s_chosen[4] ? "on" : "off",
                                 s_chosen[5] ? "on" : "off");
}
} // namespace

bool Detail::g_available = false;

bool Available()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    return Render::Look::RemasterGraphicsOptions();
#endif
}

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    Detail::g_available = Available();
    s_chosen[Index(Feature::LightPools)] = config.GetSkillLightPools() != 0;
    s_chosen[Index(Feature::SoftParticles)] = config.GetSoftParticles() != 0;
    s_chosen[Index(Feature::FlipbookBlend)] = config.GetFlipbookBlending() != 0;
    s_chosen[Index(Feature::Ribbons)] = config.GetSmoothTrails() != 0;
    s_chosen[Index(Feature::Decals)] = config.GetSkillGroundMarks() != 0;
    s_chosen[Index(Feature::Cores)] = config.GetSkillHotCores() != 0;
    Log();
}

bool Detail::EnabledWhenAvailable(const Feature feature)
{
    return Resolve(s_chosen[Index(feature)], true, feature, EffectLevel());
}

int ActivePoolBudget()
{
    return Enabled(Feature::LightPools) ? PoolBudget(EffectLevel()) : 0;
}

int ActiveRibbonPieces()
{
    return Enabled(Feature::Ribbons) ? RibbonPieceBudget(EffectLevel()) : 0;
}

int ActiveRibbonQuadBudget()
{
    return Enabled(Feature::Ribbons) ? RibbonQuadBudget(EffectLevel()) : 0;
}

int ActiveMarkBudget()
{
    return Enabled(Feature::Decals) ? MarkBudget(EffectLevel()) : 0;
}

float ActiveCoreStrength()
{
    return Enabled(Feature::Cores) ? CoreStrength(EffectLevel()) : 0.0f;
}

bool Chosen(const Feature feature)
{
    return s_chosen[Index(feature)];
}

void Choose(const Feature feature, const bool enabled)
{
    GameConfig& config = GameConfig::GetInstance();
    const int value = enabled ? 1 : 0;
    switch (feature)
    {
    case Feature::LightPools:
        config.SetSkillLightPools(value);
        break;
    case Feature::SoftParticles:
        config.SetSoftParticles(value);
        break;
    case Feature::FlipbookBlend:
        config.SetFlipbookBlending(value);
        break;
    case Feature::Ribbons:
        config.SetSmoothTrails(value);
        break;
    case Feature::Decals:
        config.SetSkillGroundMarks(value);
        break;
    case Feature::Cores:
        config.SetSkillHotCores(value);
        break;
    }
    config.Save();
    ApplyFromConfig();
}

} // namespace Render::Effects::SkillFx
