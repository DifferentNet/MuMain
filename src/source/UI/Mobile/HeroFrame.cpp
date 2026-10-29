#include "stdafx.h"
#include "UI/Mobile/HeroFrame.h"

#include "Character/CharacterManager.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/HeroGauges.h"
#include "UI/HUD/ItemHotKey.h"
#include "Network/Server/WSclient.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>

namespace UI::Mobile::HeroFrame
{
namespace
{
namespace Gauges = UI::HUD::HeroGauges;

template <typename T> void Set(T& field, const T& value, const char* name, MarkDirty markDirty)
{
    if (field == value)
        return;
    field = value;
    markDirty(name);
}

Rml::String GaugeText(const Gauges::Gauge& gauge)
{
    return std::to_string(gauge.current) + " / " + std::to_string(gauge.max);
}

void SetGauge(float& fraction, Rml::String& text, const Gauges::Gauge& gauge, const char* fractionName,
              const char* textName, MarkDirty markDirty)
{
    Set(fraction, gauge.Fraction(), fractionName, markDirty);
    Set(text, GaugeText(gauge), textName, markDirty);
}

// The level, and the master level beside it once there is one.
Rml::String LevelText()
{
    wchar_t text[64] = {};
    if (gCharacterManager.IsMasterLevel(Hero->Class))
        mu_swprintf(text, I18N::Game::TouchHeroMasterLevel, static_cast<int>(CharacterAttribute->Level),
                    static_cast<int>(Master_Level_Data.nMLevel));
    else
        mu_swprintf(text, I18N::Game::TouchHeroLevel, static_cast<int>(CharacterAttribute->Level));
    return StringUtils::WideToNarrow(text);
}

Rml::String PotionCount(int hotKey)
{
    const int count = g_pMainFrame->GetItemHotKeyCount(hotKey);
    return count > 0 ? std::to_string(count) : Rml::String();
}
} // namespace

void Bind(Rml::DataModelConstructor& constructor, Model& model)
{
    constructor.Bind("hero_hp", &model.hp);
    constructor.Bind("hero_mp", &model.mp);
    constructor.Bind("hero_sd", &model.sd);
    constructor.Bind("hero_ag", &model.ag);
    constructor.Bind("hero_exp", &model.exp);
    constructor.Bind("hero_hp_text", &model.hpText);
    constructor.Bind("hero_mp_text", &model.mpText);
    constructor.Bind("hero_sd_text", &model.sdText);
    constructor.Bind("hero_ag_text", &model.agText);
    constructor.Bind("hero_level", &model.levelText);
    constructor.Bind("potion_q_count", &model.potionQCount);
    constructor.Bind("potion_w_count", &model.potionWCount);
}

void Sync(Model& model, MarkDirty markDirty)
{
    SetGauge(model.hp, model.hpText, Gauges::Life(), "hero_hp", "hero_hp_text", markDirty);
    SetGauge(model.mp, model.mpText, Gauges::Mana(), "hero_mp", "hero_mp_text", markDirty);
    SetGauge(model.sd, model.sdText, Gauges::Shield(), "hero_sd", "hero_sd_text", markDirty);
    SetGauge(model.ag, model.agText, Gauges::SkillMana(), "hero_ag", "hero_ag_text", markDirty);
    Set(model.exp, static_cast<float>(Gauges::ReadExperience().progress), "hero_exp", markDirty);
    Set(model.levelText, LevelText(), "hero_level", markDirty);
    Set(model.potionQCount, PotionCount(mu::ui::window::HOTKEY_Q), "potion_q_count", markDirty);
    Set(model.potionWCount, PotionCount(mu::ui::window::HOTKEY_W), "potion_w_count", markDirty);
}
} // namespace UI::Mobile::HeroFrame
