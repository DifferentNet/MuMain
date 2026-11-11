#pragma once

// MU Moldova: the phone's hero frame, part of the touch controls' document (touch_controls.rml,
// shown in the phone view in place of the PC bar): HP and MP with their numbers, SD and AG as thin
// bars, the level in a circle, a thin EXP line along the bottom, and how many potions the buttons Q and W hold. The touch
// controls own the document and its model; this fills the frame's part of it.

#include "UI/HUD/HeroGauges.h"

#include <RmlUi/Core/Types.h>

#include <chrono>

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
    std::chrono::steady_clock::time_point potionsCountedAt{};
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
    Sources seen;
};

void Bind(Rml::DataModelConstructor& constructor, Model& model);

// Marks a field of the owner's model changed.
using MarkDirty = void (*)(const char* field);

// Reads the hero into the model, once a frame while the frame shows.
void Sync(Model& model, MarkDirty markDirty);
} // namespace UI::Mobile::HeroFrame
