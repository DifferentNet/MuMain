#include "stdafx.h"
#include "UI/Mobile/HeroFrame.h"

#include "Character/CharacterManager.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/ItemHotKey.h"
#include "UI/Mobile/CompactNumber.h"
#include "Network/Server/WSclient.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <string>

namespace UI::Mobile::HeroFrame
{
namespace
{
namespace Gauges = UI::HUD::HeroGauges;

// Counting the potions walks the whole inventory; a count a quarter of a second late is not seen.
constexpr auto PotionCountEvery = std::chrono::milliseconds(250);

template <typename T> void Set(T& field, const T& value, const char* name, MarkDirty markDirty)
{
    if (field == value)
        return;
    field = value;
    markDirty(name);
}

using GaugeFormat = Rml::String (*)(const Gauges::Gauge&);

// HP and MP have the whole frame's width: the numbers as they are.
Rml::String FullText(const Gauges::Gauge& gauge)
{
    return std::to_string(gauge.current) + " / " + std::to_string(gauge.max);
}

// SD and AG share a row, so each has half the width: big numbers in thousands ("32k/32k").
Rml::String ShortText(const Gauges::Gauge& gauge)
{
    return CompactNumber::Format(gauge.current) + "/" + CompactNumber::Format(gauge.max);
}

// One gauge of the frame: its model fields and how its numbers are written.
struct GaugeSlot
{
    const char* fractionName;
    const char* textName;
    GaugeFormat format;
};

constexpr GaugeSlot HpSlot{"hero_hp", "hero_hp_text", FullText};
constexpr GaugeSlot MpSlot{"hero_mp", "hero_mp_text", FullText};
constexpr GaugeSlot SdSlot{"hero_sd", "hero_sd_text", ShortText};
constexpr GaugeSlot AgSlot{"hero_ag", "hero_ag_text", ShortText};

bool Same(const Gauges::Gauge& a, const Gauges::Gauge& b)
{
    return a.current == b.current && a.max == b.max;
}

void SetGauge(float& fraction, Rml::String& text, Gauges::Gauge& seen, const Gauges::Gauge& gauge,
              const GaugeSlot& slot, bool force, MarkDirty markDirty)
{
    if (!force && Same(seen, gauge))
        return;
    seen = gauge;
    Set(fraction, gauge.Fraction(), slot.fractionName, markDirty);
    Set(text, slot.format(gauge), slot.textName, markDirty);
}

// The level, and the master level beside it once there is one.
void SyncLevel(Model& model, bool force, MarkDirty markDirty)
{
    const bool master = gCharacterManager.IsMasterLevel(Hero->Class);
    const wchar_t* format = master ? I18N::Game::TouchHeroMasterLevel : I18N::Game::TouchHeroLevel;
    const int level = static_cast<int>(CharacterAttribute->Level);
    const int masterLevel = master ? static_cast<int>(Master_Level_Data.nMLevel) : 0;
    Sources& seen = model.seen;
    if (!force && format == seen.levelFormat && level == seen.level && masterLevel == seen.masterLevel)
        return;
    seen.levelFormat = format;
    seen.level = level;
    seen.masterLevel = masterLevel;

    wchar_t text[64] = {};
    if (master)
        mu_swprintf(text, format, level, masterLevel);
    else
        mu_swprintf(text, format, level);
    Set(model.levelText, StringUtils::WideToNarrow(text), "hero_level", markDirty);
}

void SetPotions(Rml::String& text, int& seen, int count, bool force, const char* name, MarkDirty markDirty)
{
    if (!force && count == seen)
        return;
    seen = count;
    Set(text, count > 0 ? std::to_string(count) : Rml::String(), name, markDirty);
}

void SyncPotions(Model& model, bool force, MarkDirty markDirty)
{
    const auto now = std::chrono::steady_clock::now();
    Sources& seen = model.seen;
    if (!force && now - seen.potionsCountedAt < PotionCountEvery)
        return;
    seen.potionsCountedAt = now;
    SetPotions(model.potionQCount, seen.potionsQ, g_pMainFrame->GetItemHotKeyCount(mu::ui::window::HOTKEY_Q), force,
               "potion_q_count", markDirty);
    SetPotions(model.potionWCount, seen.potionsW, g_pMainFrame->GetItemHotKeyCount(mu::ui::window::HOTKEY_W), force,
               "potion_w_count", markDirty);
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
    // The first fill writes every field whatever the remembered sources say.
    const bool force = !model.seen.filled;
    model.seen.filled = true;
    Sources& seen = model.seen;
    SetGauge(model.hp, model.hpText, seen.hp, Gauges::Life(), HpSlot, force, markDirty);
    SetGauge(model.mp, model.mpText, seen.mp, Gauges::Mana(), MpSlot, force, markDirty);
    SetGauge(model.sd, model.sdText, seen.sd, Gauges::Shield(), SdSlot, force, markDirty);
    SetGauge(model.ag, model.agText, seen.ag, Gauges::SkillMana(), AgSlot, force, markDirty);
    Set(model.exp, static_cast<float>(Gauges::ReadExperience().progress), "hero_exp", markDirty);
    SyncLevel(model, force, markDirty);
    SyncPotions(model, force, markDirty);
}
} // namespace UI::Mobile::HeroFrame
