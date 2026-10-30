#pragma once

#include "Core/Globals/_types.h"

class BMD;
class CHARACTER;
class OBJECT;

// The full set's glow (RenderCharacter in Engine/Object/ZzzCharacter.cpp): lights in the armour's colour at the
// hands, flares around the character from +10, the light falling from some sets' boots and, at +15, the eyes' lights
// and trails. The game draws it for a whole set of one armour kind at +10 or higher (CheckFullSet).
//
// A character with the +14/+15 ancient tier (GameLogic/Items/AncientAura.h) gets the same glow in gold, at the
// level of its tier (or of its armour set when that is higher), also when its armour pieces aren't all of one kind;
// gold wins over the armour's own colour. The ancient set's own pulse at the feet (CHARACTER::ExtendState) keeps its
// colour: its trails (BITMAP_JOINT_HEALING subtype 9) take no light from outside.
namespace Render::Effects
{
// How a character's full-set glow is drawn this frame.
struct FullSetGlow
{
    bool Draw = false;
    // The level that picks the glow's effects: 10 to 15 (9 only the lights at the hands).
    int Level = 0;
    bool Gold = false;
};

// The glow of a character: sameKindSet and sameKindLevel are what CheckFullSet found (its EquipmentLevelSet).
FullSetGlow FullSetGlowOf(const CHARACTER* c, bool sameKindSet, int sameKindLevel);

// The colour of the lights at the hands: the armour's own (PartObjectColor of the boots) or gold.
void FullSetHandLight(const CHARACTER* c, const OBJECT* o, const FullSetGlow& glow, vec3_t light);

// The colour of the flares around the character: white, or gold.
void FullSetFlareLight(const FullSetGlow& glow, vec3_t light);

// The colour of the light falling from the boots: overrides the set's colour when gold.
void FullSetBootsLight(const FullSetGlow& glow, vec3_t light);

// The +15 glow's eye lights and trails (blue; gold when the glow is).
void RenderFullSetEyes(OBJECT* o, BMD* b, bool gold);
} // namespace Render::Effects
