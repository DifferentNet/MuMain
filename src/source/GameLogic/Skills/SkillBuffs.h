// The buff a support skill puts on (pad round 6, N2): the pad's and the touch controls' held buff
// button does not cast again while it is still on, and the button shows a thin ring meanwhile.
//
// The same skills and buffs as the MU Helper's own check (CMuHelper::BuffTarget), plus the mastery
// rank of Greater Damage that the helper's table lacks.
#pragma once

#include "Core/Globals/_enum.h"

namespace GameLogic::Skills::SkillBuffs
{
struct Buff
{
    // eBuffNone for a skill that puts on no buff this table knows.
    eBuffState state = eBuffNone;
    // The buff only ever lands on the caster (Infinity Arrow, Berserker): looked up on the hero, never
    // on the party member chosen for the heals and buffs.
    bool onCasterOnly = false;
};

[[nodiscard]] Buff BuffOf(ActionSkillType skill);
} // namespace GameLogic::Skills::SkillBuffs
