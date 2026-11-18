#include "stdafx.h"
#include "GameLogic/Skills/SkillBuffs.h"

namespace GameLogic::Skills::SkillBuffs
{
Buff BuffOf(ActionSkillType skill)
{
    switch (skill)
    {
    case AT_SKILL_ATTACK:
    case AT_SKILL_ATTACK_STR:
    case AT_SKILL_ATTACK_MASTERY:
        return {eBuff_Attack};
    case AT_SKILL_DEFENSE:
    case AT_SKILL_DEFENSE_STR:
    case AT_SKILL_DEFENSE_MASTERY:
        return {eBuff_Defense};
    case AT_SKILL_INFINITY_ARROW:
    case AT_SKILL_INFINITY_ARROW_STR:
        return {eBuff_InfinityArrow, true};
    case AT_SKILL_SOUL_BARRIER:
    case AT_SKILL_SOUL_BARRIER_STR:
    case AT_SKILL_SOUL_BARRIER_PROFICIENCY:
        return {eBuff_WizDefense};
    case AT_SKILL_SWELL_LIFE:
    case AT_SKILL_SWELL_LIFE_STR:
    case AT_SKILL_SWELL_LIFE_PROFICIENCY:
        return {eBuff_Life};
    case AT_SKILL_EXPANSION_OF_WIZARDRY:
    case AT_SKILL_EXPANSION_OF_WIZARDRY_STR:
    case AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY:
        return {eBuff_SwellOfMagicPower};
    case AT_SKILL_ADD_CRITICAL:
    case AT_SKILL_ADD_CRITICAL_STR1:
    case AT_SKILL_ADD_CRITICAL_STR2:
    case AT_SKILL_ADD_CRITICAL_STR3:
        return {eBuff_AddCriticalDamage};
    case AT_SKILL_ALICE_BERSERKER:
    case AT_SKILL_ALICE_BERSERKER_STR:
        return {eBuff_Berserker, true};
    case AT_SKILL_ALICE_THORNS:
        return {eBuff_Thorns};
    case AT_SKILL_ATT_UP_OURFORCES:
        return {eBuff_Att_up_Ourforces};
    case AT_SKILL_HP_UP_OURFORCES:
    case AT_SKILL_HP_UP_OURFORCES_STR:
        return {eBuff_Hp_up_Ourforces};
    case AT_SKILL_DEF_UP_OURFORCES:
    case AT_SKILL_DEF_UP_OURFORCES_STR:
    case AT_SKILL_DEF_UP_OURFORCES_MASTERY:
        return {eBuff_Def_up_Ourforces};
    default:
        return {};
    }
}
} // namespace GameLogic::Skills::SkillBuffs
