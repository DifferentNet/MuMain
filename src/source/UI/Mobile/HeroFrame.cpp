#include "stdafx.h"
#include "UI/Mobile/HeroFrame.h"

#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/ItemHotKey.h"
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

// One gauge of the frame: its model fields and how its numbers are written (SD and AG are thin bars
// with none).
struct GaugeSlot
{
    const char* fractionName;
    const char* textName;
    GaugeFormat format;
};

constexpr GaugeSlot HpSlot{"hero_hp", "hero_hp_text", FullText};
constexpr GaugeSlot MpSlot{"hero_mp", "hero_mp_text", FullText};

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

// The bar's fill and nothing else: SD and AG show no numbers.
void SetBar(float& fraction, Gauges::Gauge& seen, const Gauges::Gauge& gauge, const char* name, bool force,
            MarkDirty markDirty)
{
    if (!force && Same(seen, gauge))
        return;
    seen = gauge;
    Set(fraction, gauge.Fraction(), name, markDirty);
}

// The level's number in the circle, and the master level under it once there is one: numbers only, so
// the circle needs no words in any language.
void SyncLevel(Model& model, bool force, MarkDirty markDirty)
{
    const bool master = gCharacterManager.IsMasterLevel(Hero->Class);
    const int level = static_cast<int>(CharacterAttribute->Level);
    const int masterLevel = master ? static_cast<int>(Master_Level_Data.nMLevel) : 0;
    Sources& seen = model.seen;
    if (!force && level == seen.level && masterLevel == seen.masterLevel)
        return;
    seen.level = level;
    seen.masterLevel = masterLevel;
    Set(model.levelText, std::to_string(level), "hero_level", markDirty);
    Set(model.masterText, masterLevel > 0 ? "+" + std::to_string(masterLevel) : Rml::String(), "hero_master",
        markDirty);
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
    // E and R: a pad's RT page shows them.
    SetPotions(model.potionECount, seen.potionsE, g_pMainFrame->GetItemHotKeyCount(mu::ui::window::HOTKEY_E), force,
               "potion_e_count", markDirty);
    SetPotions(model.potionRCount, seen.potionsR, g_pMainFrame->GetItemHotKeyCount(mu::ui::window::HOTKEY_R), force,
               "potion_r_count", markDirty);
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
    constructor.Bind("hero_level", &model.levelText);
    constructor.Bind("hero_master", &model.masterText);
    constructor.Bind("potion_q_count", &model.potionQCount);
    constructor.Bind("potion_w_count", &model.potionWCount);
    constructor.Bind("potion_e_count", &model.potionECount);
    constructor.Bind("potion_r_count", &model.potionRCount);
}

void Sync(Model& model, MarkDirty markDirty)
{
    // The first fill writes every field whatever the remembered sources say.
    const bool force = !model.seen.filled;
    model.seen.filled = true;
    Sources& seen = model.seen;
    SetGauge(model.hp, model.hpText, seen.hp, Gauges::Life(), HpSlot, force, markDirty);
    SetGauge(model.mp, model.mpText, seen.mp, Gauges::Mana(), MpSlot, force, markDirty);
    SetBar(model.sd, seen.sd, Gauges::Shield(), "hero_sd", force, markDirty);
    SetBar(model.ag, seen.ag, Gauges::SkillMana(), "hero_ag", force, markDirty);
    Set(model.exp, static_cast<float>(Gauges::ReadExperience().progress), "hero_exp", markDirty);
    SyncLevel(model, force, markDirty);
    SyncPotions(model, force, markDirty);
}
} // namespace UI::Mobile::HeroFrame
