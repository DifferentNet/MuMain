#pragma once

#include "Core/Globals/_enum.h" // ActionSkillType

class CHARACTER;

// Skill execution dispatcher: validates a skill cast (range, mana, conditions)
// and routes it to the per-class executor. Extracted from ZzzInterface.cpp.
namespace GameLogic::Combat
{
    int  ExecuteSkill(CHARACTER* c, ActionSkillType Skill, float Distance);
    bool ExecuteSkillComplete(CHARACTER* c);
    bool CanExecuteSkill(CHARACTER* c, ActionSkillType Skill, float Distance);
    bool CheckMana(CHARACTER* c, int Skill);

    enum class CostShortfall
    {
        None,
        Mana,
        Ag,
    };

    // What the hero lacks to pay for the skill, weighed as CheckMana does but
    // without asking for a potion. None when the skill's stat requirements are
    // not met: then the cost is not the reason.
    CostShortfall SkillCostShortfall(int Skill);
}
