// SkillEffectMgr.h: interface for the CSkillEffectMgr class.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Render/Effects/PoolSlotBounds.h"

class CSkillEffectMgr
{
public:
    CSkillEffectMgr();
    virtual ~CSkillEffectMgr();

    int GetSize()
    {
        return MAX_SKILL_EFFECTS;
    }
    OBJECT* GetEffect(int iIndex);

    BOOL IsSkillEffect(int Type, vec3_t Position, vec3_t Angle, vec3_t Light, int SubType = 0, OBJECT* Target = NULL,
                       short PKKey = -1, WORD SkillIndex = 0, WORD Skill = 0, WORD SkillSerialNum = 0,
                       float Scale = 0.0f, short int sTargetIndex = -1);
    OBJECT* CreateEffect();
    // The caller of CreateEffect set the returned slot live.
    void NoteTaken(const OBJECT* o);
    // One past the highest slot that may be live.
    int SlotEnd() const
    {
        return m_bounds.End();
    }
    // How many creates took a slot so far (PoolSlotBounds::Takes).
    std::uint32_t Takes() const
    {
        return m_bounds.Takes();
    }

    bool DeleteEffect(int Type, OBJECT* Owner, int iSubType = -1);
    void DeleteEffect(int efftype);
    void DeleteAllEffects();

    bool SearchEffect(int iType, OBJECT* pOwner, int iSubType = -1);
    BOOL FindSameEffectOfSameOwner(int iType, OBJECT* pOwner);

    void MoveEffects();

protected:
    Render::Effects::PoolSlotBounds m_bounds;
    OBJECT m_SkillEffects[MAX_SKILL_EFFECTS]; // 스킬 판정에 사용하는 이펙트들 (블러드어택 등)
};

extern CSkillEffectMgr g_SkillEffects;
