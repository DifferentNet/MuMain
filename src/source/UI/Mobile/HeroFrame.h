#pragma once

// MU Moldova: the phone's hero frame, part of the touch controls' document (touch_controls.rml,
// shown in the phone view in place of the PC bar): HP, MP, SD and AG with their numbers, the
// level, a thin EXP line along the bottom, and how many potions the buttons Q and W hold. The touch
// controls own the document and its model; this fills the frame's part of it.

#include <RmlUi/Core/Types.h>

namespace Rml
{
class DataModelConstructor;
}

namespace UI::Mobile::HeroFrame
{
struct Model
{
    float hp = 0.f;
    float mp = 0.f;
    float sd = 0.f;
    float ag = 0.f;
    float exp = 0.f;
    Rml::String hpText;
    Rml::String mpText;
    Rml::String sdText;
    Rml::String agText;
    Rml::String levelText;
    Rml::String potionQCount;
    Rml::String potionWCount;
};

void Bind(Rml::DataModelConstructor& constructor, Model& model);

// Marks a field of the owner's model changed.
using MarkDirty = void (*)(const char* field);

// Reads the hero into the model, once a frame while the frame shows.
void Sync(Model& model, MarkDirty markDirty);
} // namespace UI::Mobile::HeroFrame
