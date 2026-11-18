#pragma once

// MU Moldova: the phone's hero frame, part of the touch controls' document (touch_controls.rml,
// shown in the phone view in place of the PC bar): HP and MP with their numbers, SD and AG as thin
// bars, the level in a circle, a thin EXP line along the bottom, and how many potions the buttons Q and W hold. The touch
// controls own the document and its model; this fills the frame's part of it.

#include "UI/HUD/HeroGauges.h"

#include <RmlUi/Core/Types.h>

#include <chrono>
#include <string>

namespace Rml
{
class DataModelConstructor;
}

namespace UI::Mobile::HeroFrame
{
// What the frame's texts were last made from. Sync runs every frame; a text is only made again
// when what it shows changed, and the potions (a walk over the whole inventory) are counted a few
// times a second, not every frame.
struct Sources
{
    bool filled = false;
    UI::HUD::HeroGauges::Gauge hp;
    UI::HUD::HeroGauges::Gauge mp;
    UI::HUD::HeroGauges::Gauge sd;
    UI::HUD::HeroGauges::Gauge ag;
    int level = 0;
    int masterLevel = 0;
    int potionsQ = 0;
    int potionsW = 0;
    // E and R: the pad drinks them with RT held (UI/HUD/GamepadControls).
    int potionsE = 0;
    int potionsR = 0;
    // What Q and W hold (UI/Mobile/PotionKind.h).
    int potionKindQ = 0;
    int potionKindW = 0;
    std::chrono::steady_clock::time_point potionsCountedAt{};
    // The EXP percentage in tenths of a percent (427 for 42.7 %); -1 before the first.
    int expTenths = -1;
    // The time to the next level shown after it, when the pace is known; looked at once a second.
    std::wstring expTimeToLevel;
    std::chrono::steady_clock::time_point expPaceCheckedAt{};
};

struct Model
{
    float hp = 0.f;
    float mp = 0.f;
    float sd = 0.f;
    float ag = 0.f;
    float exp = 0.f;
    Rml::String hpText;
    Rml::String mpText;
    // The level's number alone, and the master level as "+N" (empty without one) under it.
    Rml::String levelText;
    Rml::String masterText;
    Rml::String potionQCount;
    Rml::String potionWCount;
    Rml::String potionECount;
    Rml::String potionRCount;
    // The kind of potion on Q and W (UI::Mobile::PotionKind as int): the buttons are tinted by it.
    int potionQKind = 0;
    int potionWKind = 0;
    // Free stat points to spend: a "+" on the level's circle.
    bool pointsAlert = false;
    // The EXP line's percentage with one decimal ("42.7%"), the phone HUD's text beside the line, and after a minute
    // of hunting the time to the next level after a middle dot, as the PC's EXP tooltip shows it.
    Rml::String expText;
    Sources seen;
};

void Bind(Rml::DataModelConstructor& constructor, Model& model);

// Marks a field of the owner's model changed.
using MarkDirty = void (*)(const char* field);

// Reads the hero into the model, once a frame while the frame shows.
void Sync(Model& model, MarkDirty markDirty);
} // namespace UI::Mobile::HeroFrame
