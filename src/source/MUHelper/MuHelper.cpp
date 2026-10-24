#include "stdafx.h"
#include "GameLogic/Automation/Attack.h"
#include "GameLogic/Automation/Movement.h"
#include "GameLogic/Automation/Pickup.h"
#include "GameLogic/Automation/Skill.h"
#include "GameLogic/Combat/SkillExecution.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>

#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/PlayerActionState.h"
#include "UI/Core/WindowSystem.h"
#include "Core/Utilities/Log/muConsoleDebug.h"
#include "Character/CharacterManager.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Social/PartyManager.h"
#include "World/MapInfra/MapManager.h"
#include "Network/Server/WSclient.h"
#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/ShopRestrictions.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"

#include "MuHelper.h"

constexpr int MAX_ACTIONABLE_DISTANCE = 10;
constexpr int DEFAULT_DURABILITY_THRESHOLD = 50;
// Pickups of one drop the server refused before the helper gives the drop up (see ObtainItem).
constexpr int MAX_PICKUP_ATTEMPTS = 5;

SpinLock _targetsLock;
SpinLock _itemsLock;

// Movement/target globals are defined in ZzzInterface.cpp.
extern MovementSkill g_MovementSkill;
extern int SelectedCharacter;
extern int TargetX;
extern int TargetY;

namespace MUHelper
{
	MovementSkill& g_MovementSkill = ::g_MovementSkill;
	int& SelectedCharacter = ::SelectedCharacter;
	int& TargetX = ::TargetX;
	int& TargetY = ::TargetY;

    CMuHelper g_MuHelper;

    void CALLBACK CMuHelper::TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        g_MuHelper.WorkLoop(hwnd, uMsg, idEvent, dwTime);
    }

    void CMuHelper::Save(const ConfigData& config)
    {
        m_config = config;

        PRECEIVE_MUHELPER_DATA netData;
        ConfigDataSerDe::Serialize(m_config, netData);

        SocketClient->ToGameServer()->SendMuHelperSaveDataRequest(reinterpret_cast<BYTE*>(&netData), sizeof(netData));
    }

    void CMuHelper::Load(const ConfigData& config)
    {
        m_config = config;
    }

    ConfigData CMuHelper::GetConfig() const {
        return m_config;
    }

    void CMuHelper::Toggle()
    {
        if (m_bActive)
        {
            TriggerStop();

            // Stop the client-driven bot immediately instead of waiting for the
            // server's status reply. After an auto-reconnect the server's new
            // session doesn't have the helper marked active, so it never replies
            // and the bot would otherwise keep running with no way to stop it.
            Stop();
        }
        else
        {
            TriggerStart();
        }
    }

    void CMuHelper::TriggerStart()
    {
        if (!Hero->SafeZone)
            SocketClient->ToGameServer()->SendMuHelperStatusChangeRequest(0);
    }

    void CMuHelper::TriggerStop()
    {
        SocketClient->ToGameServer()->SendMuHelperStatusChangeRequest(1);
    }

    void CMuHelper::Start()
    {
        if (m_bActive)
        {
            return;
        }

        m_iTotalCost = 0;
        m_iComboState = 0;
        m_iCurrentBuffIndex = 0;
        m_iCurrentBuffPartyIndex = 0;
        m_iCurrentHealPartyIndex = 0;
        m_iCurrentTarget = -1;
        m_iCurrentSkill = (ActionSkillType)m_config.aiSkill[0];
        m_iCurrentItem = MAX_ITEMS;
        m_posOriginal = { Hero->PositionX, Hero->PositionY };

        m_iHuntingDistance = ComputeDistanceByRange(m_config.iHuntingRange);
        m_iObtainingDistance = ComputeDistanceByRange(m_config.iObtainingRange);

        m_iSecondsElapsed = 0;
        m_iSecondsAway = 0;

        m_bTimerActivatedBuffOngoing = false;
        m_bPetActivated = false;

        m_iLoopCounter = 0;
        m_nextRound = {};
        m_bAttackBetweenRounds = false;
        m_aiTimedSkillSecond.fill(-1);
        m_iTimedSkillSlot = -1;
        m_mapSupportFailures.clear();
        m_mapSuspendedUntil.clear();
        m_bManaShort = false;
        m_setNotified.clear();
        m_mapForgottenUntil.clear();
        m_noTargetInReachSince = {};
        m_walkWatchSince = {};

        m_bActive = true;
        g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Started");
    }

    void CMuHelper::Stop()
    {
        m_bActive = false;
        g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Stopped");
    }

    void CMuHelper::WorkLoop(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        if (!m_bActive)
        {
            return;
        }

        if (Hero->SafeZone)
        {
            g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Entered safezone. Stopping.");
            TriggerStop();
            return;
        }

        // The game calls this on every frame. The whole round (pet, buffs, potions, pickup, the walk back, the
        // attack, repairs) runs every RoundInterval, as it always did, so the server's answer to a potion, a buff
        // or a pickup arrives before the next. Between the rounds only the attack runs, as long as the last round
        // got as far as it: with a round per cast the helper made at most four casts a second, while a player at
        // attack speed 1000 casts 10 to 17 by hand (2026-10-22). The attack itself waits for the swing to end.
        const auto now = std::chrono::steady_clock::now();
        if (now < m_nextRound)
        {
            // Not while walking: a walk towards a target out of reach is planned again on every call, and on
            // every frame that restarted the step and sent the server a new walk each time, so the character
            // stood or jerked back and forth instead of getting there (players, 2026-10-22). The walk goes on
            // between the rounds; the next round looks again.
            if (m_bAttackBetweenRounds && !Hero->Movement)
            {
                try
                {
                    Attack();
                }
                catch (...)
                {
                    g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Exception occurred. Ignoring...");
                }
            }
            return;
        }
        m_nextRound = now + RoundInterval;

        StopStuckWalk(now);
        Work();

        if (m_iLoopCounter++ == 4)
        {
            m_iSecondsElapsed++;

            if (ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, m_posOriginal) > 1)
            {
                m_iSecondsAway++;
            }
            else
            {
                m_iSecondsAway = 0;
            }

            m_iLoopCounter = 0;
        }
    }

    void CMuHelper::StopStuckWalk(std::chrono::steady_clock::time_point now)
    {
        // A walk that goes nowhere, its next tile taken by a monster in a crowd, keeps Hero->Movement set, and no
        // cast completes while it is (ExecuteSkillComplete). Once the helper also walks up to monsters when standing
        // still (2026-10-23), such a walk must not freeze it: after StuckWalkLimit on the same tile it is stopped.
        if (!Hero->Movement)
        {
            m_walkWatchSince = {};
            return;
        }
        if (m_walkWatchSince == std::chrono::steady_clock::time_point{}
            || Hero->PositionX != m_posWalkWatch.x || Hero->PositionY != m_posWalkWatch.y)
        {
            m_posWalkWatch = { Hero->PositionX, Hero->PositionY };
            m_walkWatchSince = now;
            return;
        }
        if (now - m_walkWatchSince < StuckWalkLimit)
        {
            return;
        }

        Hero->Path.Lock.lock();
        Hero->Path.PathNum = 0;
        Hero->Path.CurrentPath = 0;
        Hero->Path.CurrentPathFloat = 0;
        Hero->Path.Lock.unlock();
        LetHeroStop(Hero, TRUE);
        m_walkWatchSince = {};
        m_iCurrentTarget = -1;
        g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Stopped a walk that went nowhere");
    }

    void CMuHelper::Work()
    {
        m_bAttackBetweenRounds = false;
        try
        {
            RefreshTargets();

            if (!ActivatePet())
            {
                return;
            }

            if (!Buff())
            {
                return;
            }

            if (!RecoverHealth())
            {
                return;
            }

            if (!ObtainItem())
            {
                return;
            }

            if (!Regroup())
            {
                return;
            }

            m_bAttackBetweenRounds = true;
            Attack();

            RepairEquipments();
        }
        catch (...)
        {
            g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Exception occurred. Ignoring...");
        }
    }

    void CMuHelper::AddTarget(int iTargetId, bool bIsAttacking)
    {
        if (!m_bActive)
        {
            return;
        }

        CHARACTER* pTarget = FindCharacterByKey(iTargetId);
        if (!pTarget || pTarget == Hero)
        {
            return;
        }

        int iDistance = ComputeDistanceFromTarget(pTarget);

        if ((iDistance <= m_iHuntingDistance)
            || (bIsAttacking && m_config.bLongRangeCounterAttack))
        {
            _targetsLock.lock();

            m_setTargets.insert(iTargetId);

            if (bIsAttacking)
            {
                m_setTargetsAttacking.insert(iTargetId);
            }

            _targetsLock.unlock();
        }

        if (m_config.bUseSelfDefense && IsMonster(pTarget))
        {
            m_iCurrentTarget = iTargetId;
        }
    }

    void CMuHelper::DeleteTarget(int iTargetId)
    {
        _targetsLock.lock();

        m_setTargets.erase(iTargetId);
        m_setTargetsAttacking.erase(iTargetId);

        _targetsLock.unlock();

        if (iTargetId == m_iCurrentTarget)
        {
            m_iCurrentTarget = -1;
        }
    }

    void CMuHelper::DeleteAllTargets()
    {
        _targetsLock.lock();

        m_setTargets.clear();
        m_setTargetsAttacking.clear();

        _targetsLock.unlock();
    }

    int CMuHelper::ComputeDistanceByRange(int iRange)
    {
        return ComputeDistanceBetween({ 0, 0 }, { iRange, iRange });
    }

    int CMuHelper::ComputeDistanceFromTarget(CHARACTER* pTarget)
    {
        const POINT posHero = { Hero->PositionX, Hero->PositionY };

        const POINT posCurrent = { pTarget->PositionX, pTarget->PositionY };
        const POINT posNext    = { pTarget->TargetX,   pTarget->TargetY };

        return std::min(
            ComputeDistanceBetween(posHero, posCurrent),
            ComputeDistanceBetween(posHero, posNext)
        );
    }

    int CMuHelper::ComputeDistanceBetween(POINT posA, POINT posB)
    {
        int iDx = posA.x - posB.x;
        int iDy = posA.y - posB.y;

        return static_cast<int>(std::ceil(std::sqrt(iDx * iDx + iDy * iDy)));
    }

    void CMuHelper::RefreshTargets()
    {
        // The packets that add targets (a monster appearing, moving, hurt or attacking) say nothing about one
        // that just stands there: one already around when the helper started, one fighting someone else, or one
        // given up after a failed cast. The helper then stood among monsters until one moved (ghinzuro in the
        // Arena, 2026-10-23). So every round the monsters in view within the hunting range join the targets.
        const auto now = std::chrono::steady_clock::now();
        for (auto it = m_mapForgottenUntil.begin(); it != m_mapForgottenUntil.end();)
        {
            it = now < it->second ? std::next(it) : m_mapForgottenUntil.erase(it);
        }

        std::vector<int> found;
        for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
        {
            CHARACTER* pChar = &CharactersClient[i];
            if (!pChar->Object.Live || pChar->Dead > 0 || pChar == Hero || !IsMonster(pChar))
            {
                continue;
            }
            if (m_mapForgottenUntil.count(pChar->Key) || ComputeDistanceFromTarget(pChar) > m_iHuntingDistance)
            {
                continue;
            }
            found.push_back(pChar->Key);
        }

        if (found.empty())
        {
            return;
        }
        _targetsLock.lock();
        m_setTargets.insert(found.begin(), found.end());
        _targetsLock.unlock();
    }

    void CMuHelper::ForgetTarget(int iTargetId, GameLogic::Automation::SkillResult result)
    {
        using GameLogic::Automation::SkillResult;
        // Only for what is about the target. A refused skill (no mana, too little energy) used to drop the
        // target too, though the monster was still standing right there.
        if (result == SkillResult::NotInView || result == SkillResult::NotAttackable
            || result == SkillResult::NoPath || result == SkillResult::Blocked)
        {
            DeleteTarget(iTargetId);
            m_mapForgottenUntil[iTargetId] = std::chrono::steady_clock::now() + ForgottenTargetRest;
        }
    }

    int CMuHelper::GetNearestTarget()
    {
        int iClosestMonsterId = -1;
        int iMinDistance = m_iHuntingDistance;
        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargets;
            _targetsLock.unlock();
        }

        for (const int& iMonsterId : setTargets)
        {
            int iIndex = FindCharacterIndex(iMonsterId);
            if (iIndex == MAX_CHARACTERS_CLIENT)
            {
                continue;
            }
            CHARACTER* pTarget = &CharactersClient[iIndex];

            if (!IsMonster(pTarget) || pTarget->Dead > 0 || !pTarget->Object.Live)
            {
                continue;
            }

            int iDistance = ComputeDistanceFromTarget(pTarget);
            if (iDistance <= iMinDistance)
            {
                iMinDistance = iDistance;
                iClosestMonsterId = iMonsterId;
            }
        }

        return iClosestMonsterId;
    }

    int CMuHelper::GetFarthestAttackingTarget()
    {
        int iFarthestMonsterId = -1;
        int iMaxDistance = -1;

        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargetsAttacking;
            _targetsLock.unlock();
        }

        for (const int& iMonsterId : setTargets)
        {
            int iIndex = FindCharacterIndex(iMonsterId);
            if (iIndex == MAX_CHARACTERS_CLIENT)
            {
                continue;
            }
            CHARACTER* pTarget = &CharactersClient[iIndex];

            if (!IsMonster(pTarget) || pTarget->Dead > 0 || !pTarget->Object.Live)
            {
                continue;
            }

            int iDistance = ComputeDistanceFromTarget(pTarget);
            if (iDistance > iMaxDistance)
            {
                iMaxDistance = iDistance;
                iFarthestMonsterId = iMonsterId;
            }
        }

        return iFarthestMonsterId;
    }

    void CMuHelper::CleanupTargets()
    {
        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargets;
            _targetsLock.unlock();
        }

        for (const int& iMonsterId : setTargets)
        {
            int iIndex = FindCharacterIndex(iMonsterId);
            if (iIndex == MAX_CHARACTERS_CLIENT)
            {
                DeleteTarget(iMonsterId);
                continue;
            }

            CHARACTER* pTarget = &CharactersClient[iIndex];
            if (pTarget->Dead > 0 || !pTarget->Object.Live)
            {
                DeleteTarget(iMonsterId);
            }
        }
    }

    int CMuHelper::ActivatePet()
    {
        if (!m_config.bUseDarkRaven)
        {
            return 1;
        }

        if (m_bPetActivated)
        {
            return 1;
        }

        if (m_config.iDarkRavenMode == PET_ATTACK_CEASE)
        {
            SocketClient->ToGameServer()->SendPetCommandRequest(PetType::DarkRaven, PetCommandMode::Normal, 0xFFFF);
        }
        else if (m_config.iDarkRavenMode == PET_ATTACK_AUTO)
        {
            SocketClient->ToGameServer()->SendPetCommandRequest(PetType::DarkRaven, PetCommandMode::AttackRandom, 0xFFFF);
        }
        else if (m_config.iDarkRavenMode == PET_ATTACK_TOGETHER)
        {
            SocketClient->ToGameServer()->SendPetCommandRequest(PetType::DarkRaven, PetCommandMode::AttackWithOwner, 0xFFFF);
        }

        m_bPetActivated = true;
        return 1;
    }

    int CMuHelper::Buff()
    {
        if (!HasAssignedBuffSkill())
        {
            return 1;
        }

        if (m_config.bSupportParty && g_pPartyManager->IsPartyActive())
        {
            m_iCurrentBuffPartyIndex %= PartyNumber;

            PARTY_t* pMember = &Party[m_iCurrentBuffPartyIndex];
            CHARACTER* pChar = g_pPartyManager->GetPartyMemberChar(pMember);
            int iBuffResult = 1;

            if (pChar != NULL
                && ComputeDistanceFromTarget(pChar) <= MAX_ACTIONABLE_DISTANCE)
            {
                if (!m_config.bBuffDurationParty
                    && m_config.iBuffCastInterval != 0
                    && m_iSecondsElapsed % m_config.iBuffCastInterval == 0)
                {
                    m_bTimerActivatedBuffOngoing = true;
                }

                iBuffResult = BuffTarget(pChar, (ActionSkillType)m_config.aiBuff[m_iCurrentBuffIndex]);
            }

            m_iCurrentBuffPartyIndex = (m_iCurrentBuffPartyIndex + 1) % PartyNumber;

            if (m_iCurrentBuffPartyIndex == 0)
            {
                m_iCurrentBuffIndex = (m_iCurrentBuffIndex + 1) % m_config.aiBuff.size();

                // Reaching this branch means everyone's been buffed, 
                // so we're resetting the timer activated buff flag
                if (m_iCurrentBuffIndex == 0)
                {
                    m_bTimerActivatedBuffOngoing = false;
                }
            }

            return iBuffResult;
        }
        else
        {
            m_iCurrentBuffPartyIndex = 0;

            if (!m_config.bBuffDuration
                && m_config.iBuffCastInterval != 0
                && m_iSecondsElapsed % m_config.iBuffCastInterval == 0)
            {
                m_bTimerActivatedBuffOngoing = true;
            }

            if (!BuffTarget(Hero, (ActionSkillType)m_config.aiBuff[m_iCurrentBuffIndex]))
            {
                return 0;
            }
        }

        if (m_iCurrentBuffPartyIndex == 0)
        {
            m_iCurrentBuffIndex = (m_iCurrentBuffIndex + 1) % m_config.aiBuff.size();

            // Reaching this branch means everyone's been buffed, 
            // so we're resetting the timer activated buff flag
            if (m_iCurrentBuffIndex == 0)
            {
                m_bTimerActivatedBuffOngoing = false;
            }
        }

        return 1;
    }

    int CMuHelper::BuffTarget(CHARACTER* pTargetChar, ActionSkillType iBuffSkill)
    {
        OBJECT* obj = &pTargetChar->Object;

        auto CastIfMissing = [&](bool bBuffActive, bool bTimerRespected, bool bNeedsTarget) -> int
        {
            if (!bBuffActive || (bTimerRespected && m_bTimerActivatedBuffOngoing))
                return CastSupport(iBuffSkill, bNeedsTarget, pTargetChar->Key);
            return 1;
        };

        switch (iBuffSkill)
        {
        case AT_SKILL_ATTACK:
        case AT_SKILL_ATTACK_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Attack), true, true);

        case AT_SKILL_DEFENSE:
        case AT_SKILL_DEFENSE_STR:
        case AT_SKILL_DEFENSE_MASTERY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Defense), true, true);

        case AT_SKILL_INFINITY_ARROW:
        case AT_SKILL_INFINITY_ARROW_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_InfinityArrow), false, false);

        case AT_SKILL_SOUL_BARRIER:
        case AT_SKILL_SOUL_BARRIER_STR:
        case AT_SKILL_SOUL_BARRIER_PROFICIENCY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_WizDefense), true, true);

        case AT_SKILL_SWELL_LIFE:
        case AT_SKILL_SWELL_LIFE_STR:
        case AT_SKILL_SWELL_LIFE_PROFICIENCY:
            if (m_iComboState == 2)
            {
                return 1;
            }
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Life), true, false);

        case AT_SKILL_EXPANSION_OF_WIZARDRY:
        case AT_SKILL_EXPANSION_OF_WIZARDRY_STR:
        case AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_SwellOfMagicPower), false, false);

        case AT_SKILL_ADD_CRITICAL:
        case AT_SKILL_ADD_CRITICAL_STR1:
        case AT_SKILL_ADD_CRITICAL_STR2:
        case AT_SKILL_ADD_CRITICAL_STR3:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_AddCriticalDamage), false, false);

        case AT_SKILL_ALICE_BERSERKER:
        case AT_SKILL_ALICE_BERSERKER_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Berserker), false, false);

        case AT_SKILL_ALICE_THORNS:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Thorns), false, false);

        // Rage Fighter party buffs — self/party AoE, no explicit target needed.
        case AT_SKILL_ATT_UP_OURFORCES:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Att_up_Ourforces), true, false);

        case AT_SKILL_HP_UP_OURFORCES:
        case AT_SKILL_HP_UP_OURFORCES_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Hp_up_Ourforces), true, false);

        case AT_SKILL_DEF_UP_OURFORCES:
        case AT_SKILL_DEF_UP_OURFORCES_STR:
        case AT_SKILL_DEF_UP_OURFORCES_MASTERY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Def_up_Ourforces), true, false);

        default:
            return 1;
        }
    }


    int CMuHelper::ConsumePotion()
    {
        int64_t iLife = CharacterAttribute->Life;
        int64_t iLifeMax = CharacterAttribute->LifeMax;
        bool bDrank = false;

        if (m_config.bUseHealPotion && iLifeMax > 0 && iLife > 0)
        {
            int64_t iRemaining = (iLife * 100 + iLifeMax - 1) / iLifeMax;
            if (iRemaining <= m_config.iPotionThreshold)
            {
                int iPotionIndex = g_pMyInventory->FindHealingItemIndex();
                if (iPotionIndex != -1)
                {
                    SendRequestUse(iPotionIndex, 0);
                    bDrank = true;
                }
            }
        }

        // Mana potions, under the same "potion" switch, one potion a round with healing first (2026-10-23). The
        // client's own cast drinks one only after a cast failed for mana, and the helper's skills and buffs, now
        // up to 17 casts a second, emptied the mana faster than that kept up.
        const bool bManaShort = m_bManaShort;
        m_bManaShort = false;
        const int64_t iMana = CharacterAttribute->Mana;
        const int64_t iManaMax = CharacterAttribute->ManaMax;
        if (m_config.bUseHealPotion && !bDrank && iManaMax > 0
            && (bManaShort || iMana * 100 <= iManaMax * ManaPotionPercent))
        {
            const int iPotionIndex = g_pMyInventory->FindManaItemIndex();
            if (iPotionIndex != -1)
            {
                SendRequestUse(iPotionIndex, 0);
            }
            else if (bManaShort)
            {
                NotifyOnce(-1, I18N::Game::MuHelperNoManaPotions);
            }
        }

        return 1;
    }

    CMuHelper::SkillBlock CMuHelper::GetSkillBlock(ActionSkillType iSkill) const
    {
        if (iSkill <= AT_SKILL_UNDEFINED || iSkill >= MAX_SKILLS)
        {
            return SkillBlock::None;
        }

        // As the class attacks check it (ClassAttack.cpp): the energy with the gear's bonus.
        int iEnergy = 0;
        gSkillManager.GetSkillInformation_Energy(iSkill, &iEnergy);
        if (iEnergy > CharacterAttribute->Energy + CharacterAttribute->AddEnergy)
        {
            return SkillBlock::Energy;
        }

        // As AttackKnight and CanExecuteSkill check it: the leadership with the gear's bonus. A Dark Lord keeps his
        // skills through /resetstats but not the leadership they ask for (Critical Damage 300, Fire Scream 70,
        // Electric Spike 340). The client refused such a cast without a word, and the helper, which didn't see why,
        // waited 3 s for the buff every 13 s, and stood still for good with such an attack skill and the plain
        // attack fallback off (2026-10-24).
        int iCharisma = 0;
        gSkillManager.GetSkillInformation_Charisma(iSkill, &iCharisma);
        if (iCharisma > CharacterAttribute->Charisma + CharacterAttribute->AddCharisma)
        {
            return SkillBlock::Leadership;
        }

        // The rest of the stats CanExecuteSkill asks for (strength, dexterity), so nothing it refuses for them
        // comes as a surprise either.
        if (!gSkillManager.AreSkillAttributeRequirementsMet(iSkill))
        {
            return SkillBlock::Requirements;
        }

        // As CheckMana does, with the elf's Infinity Arrow surcharge from AttackElf.
        int iMana = 0;
        int iSkillMana = 0;
        gSkillManager.GetSkillInformation(iSkill, 1, nullptr, &iMana, nullptr, &iSkillMana);
        OBJECT* pHeroObject = &Hero->Object;
        if (g_isCharacterBuff(pHeroObject, eBuff_InfinityArrow))
        {
            iMana += CharacterMachine->InfinityArrowAdditionalMana;
        }
        if (static_cast<int64_t>(iMana) > static_cast<int64_t>(CharacterAttribute->Mana))
        {
            return SkillBlock::Mana;
        }
        if (static_cast<int64_t>(iSkillMana) > static_cast<int64_t>(CharacterAttribute->SkillMana))
        {
            return SkillBlock::Ability;
        }
        return SkillBlock::None;
    }

    bool CMuHelper::IsSkillSuspended(ActionSkillType iSkill) const
    {
        const auto it = m_mapSuspendedUntil.find(iSkill);
        return it != m_mapSuspendedUntil.end() && std::chrono::steady_clock::now() < it->second;
    }

    int CMuHelper::CastSupport(ActionSkillType iSkill, bool bTargetRequired, int iTarget)
    {
        // A buff or heal the character can't cast is passed over, and the round goes on to the potions and the
        // attack. Before, its failed cast ended the round, every round, so the helper neither attacked nor drank
        // (2026-10-23: Xeed's elf, Greater Defense and Greater Damage need more energy than his own 15).
        if (IsSkillSuspended(iSkill))
        {
            return 1;
        }

        const SkillBlock block = GetSkillBlock(iSkill);
        if (block != SkillBlock::None)
        {
            if (block == SkillBlock::Mana)
            {
                m_bManaShort = true;
            }
            NotifySkillBlocked(iSkill, block, false);
            return 1;
        }

        if (SimulateSkill(iSkill, bTargetRequired, iTarget))
        {
            m_mapSupportFailures.erase(iSkill);
            return 1;
        }

        using GameLogic::Automation::SkillResult;
        if (m_lastSkillResult == SkillResult::Busy || m_lastSkillResult == SkillResult::Approaching)
        {
            return 0;
        }

        // Refused for a reason the checks above don't see (a cooldown, the weapon, the map): the round goes on to
        // the potions and the attack all the same, and the next round tries again. It used to end the round here
        // for up to SupportFailureLimit rounds (3 s), and again after every rest, with neither an attack nor a
        // potion meanwhile (2026-10-24). One that keeps failing rests, so it isn't tried four times a second.
        if (++m_mapSupportFailures[iSkill] >= SupportFailureLimit)
        {
            m_mapSupportFailures.erase(iSkill);
            m_mapSuspendedUntil[iSkill] = std::chrono::steady_clock::now() + SupportSuspendTime;
            wchar_t szName[MAX_SKILL_NAME + 1] = {};
            gSkillManager.GetSkillInformation(iSkill, 1, szName, nullptr, nullptr);
            wchar_t szText[256];
            mu_swprintf(szText, I18N::Game::MuHelperSkillPaused, szName, static_cast<int>(SupportSuspendTime.count()));
            NotifyOnce(iSkill * 8 + 7, szText);
        }
        return 1;
    }

    void CMuHelper::NotifySkillBlocked(ActionSkillType iSkill, SkillBlock block, bool bPlainAttack)
    {
        const int iKey = iSkill * 8 + static_cast<int>(block);
        if (m_setNotified.count(iKey))
        {
            return;
        }

        wchar_t szName[MAX_SKILL_NAME + 1] = {};
        gSkillManager.GetSkillInformation(iSkill, 1, szName, nullptr, nullptr);
        wchar_t szText[256];
        switch (block)
        {
        case SkillBlock::Energy:
        {
            int iEnergy = 0;
            gSkillManager.GetSkillInformation_Energy(iSkill, &iEnergy);
            const int iHave = CharacterAttribute->Energy + CharacterAttribute->AddEnergy;
            mu_swprintf(szText, bPlainAttack ? I18N::Game::MuHelperSkillEnergyPlain : I18N::Game::MuHelperSkillEnergySkip,
                szName, iEnergy, iHave);
            break;
        }
        case SkillBlock::Mana:
            // Mana comes back, by itself or from a potion: only say it when there is no potion to drink.
            if (g_pMyInventory->FindManaItemIndex() != -1)
            {
                return;
            }
            mu_swprintf(szText, bPlainAttack ? I18N::Game::MuHelperSkillManaPlain : I18N::Game::MuHelperSkillManaWait, szName);
            break;
        case SkillBlock::Ability:
            mu_swprintf(szText, bPlainAttack ? I18N::Game::MuHelperSkillAgPlain : I18N::Game::MuHelperSkillAgWait, szName);
            break;
        case SkillBlock::Leadership:
        {
            int iCharisma = 0;
            gSkillManager.GetSkillInformation_Charisma(iSkill, &iCharisma);
            const int iHave = CharacterAttribute->Charisma + CharacterAttribute->AddCharisma;
            mu_swprintf(szText, bPlainAttack ? I18N::Game::MuHelperSkillLeadershipPlain : I18N::Game::MuHelperSkillLeadershipSkip,
                szName, iCharisma, iHave);
            break;
        }
        case SkillBlock::Requirements:
            mu_swprintf(szText, bPlainAttack ? I18N::Game::MuHelperSkillStatsPlain : I18N::Game::MuHelperSkillStatsSkip, szName);
            break;
        default:
            return;
        }
        NotifyOnce(iKey, szText);
    }

    void CMuHelper::NotifyOnce(int iKey, const wchar_t* pszText)
    {
        if (!m_setNotified.insert(iKey).second)
        {
            return;
        }
        g_pChatListBox->AddText(L"", pszText, mu::ui::window::TYPE_SYSTEM_MESSAGE);
        g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] %ls", pszText);
    }

    int CMuHelper::RecoverHealth()
    {
        // The potions first, every round: a heal skill that failed used to end the round before them, and the
        // character stood there being hit with its potions in the bag (players, 2026-10-23).
        ConsumePotion();

        if (!Heal())
        {
            return 0;
        }

        if (!DrainLife())
        {
            return 0;
        }

        return 1;
    }

    int CMuHelper::Heal()
    {
        if (!m_config.bAutoHeal)
        {
            return 1;
        }

        auto iHealingSkill = GetHealingSkill();
        if (iHealingSkill == AT_SKILL_UNDEFINED)
        {
            return 1;
        }

        if (m_config.bAutoHealParty && g_pPartyManager->IsPartyActive())
        {
            m_iCurrentHealPartyIndex %= PartyNumber;

            PARTY_t* pMember = &Party[m_iCurrentHealPartyIndex];
            CHARACTER* pChar = g_pPartyManager->GetPartyMemberChar(pMember);
            int iHealResult = 1;

            if (pChar != NULL)
            {
                if (pChar == Hero)
                {
                    iHealResult = HealSelf(iHealingSkill);
                }
                else if (pMember->stepHP * 10 <= m_config.iHealPartyThreshold
                    && ComputeDistanceFromTarget(pChar) <= MAX_ACTIONABLE_DISTANCE)
                {
                    iHealResult = CastSupport(iHealingSkill, true, pChar->Key);
                }
            }

            m_iCurrentHealPartyIndex = (m_iCurrentHealPartyIndex + 1) % PartyNumber;

            return iHealResult;
        }
        else
        {
            m_iCurrentHealPartyIndex = 0;
            return HealSelf(iHealingSkill);
        }

        return 1;
    }

    int CMuHelper::HealSelf(ActionSkillType iHealingSkill)
    {
        int64_t iLife = CharacterAttribute->Life;
        int64_t iLifeMax = CharacterAttribute->LifeMax;
        int64_t iRemaining = (iLife * 100 + iLifeMax - 1) / iLifeMax;

        if (iRemaining <= m_config.iHealThreshold)
        {
            return CastSupport(iHealingSkill, true, HeroKey);
        }

        return 1;
    }

    int CMuHelper::DrainLife()
    {
        if (!m_config.bUseDrainLife)
        {
            return 1;
        }

        auto iDrainLife = GetDrainLifeSkill();
        if (iDrainLife == AT_SKILL_UNDEFINED)
        {
            return 1;
        }

        int64_t iLife = CharacterAttribute->Life;
        int64_t iLifeMax = CharacterAttribute->LifeMax;
        int64_t iRemaining = (iLife * 100 + iLifeMax - 1) / iLifeMax;

        if (iRemaining <= m_config.iHealThreshold)
        {
            m_iCurrentTarget = GetNearestTarget();
            if (m_iCurrentTarget != -1)
            {
                return CastSupport(iDrainLife, true, m_iCurrentTarget);
            }
        }

        return 1;
    }

    int CMuHelper::RepairEquipments()
    {
        if (m_config.bRepairItem)
        {
            for (int i = 0; i < MAX_EQUIPMENT; i++)
            {
                ITEM* pItem = &CharacterMachine->Equipment[i];
                if (!pItem || pItem->Type == -1)
                {
                    continue;
                }

                // Arrows and bolts count their shots as durability but can't be repaired: without this,
                // a low quiver made the helper send a refused repair request every tick.
                if (GameLogic::Items::IsRepairBan(pItem))
                {
                    continue;
                }

                ITEM_ATTRIBUTE* pAttr = &ItemAttribute[pItem->Type];
                if (!pAttr)
                {
                    continue;
                }

                int iLevel = pItem->Level;
                int iDurability = pItem->Durability;
                int iMaxDurability = CalcMaxDurability(pItem, pAttr, iLevel);

                int64_t iHealth = (iDurability * 100 + iMaxDurability - 1) / iMaxDurability;

                if (iHealth <= DEFAULT_DURABILITY_THRESHOLD)
                {
                    int64_t iGoldCost = CalcSelfRepairCost(ItemValue(pItem, 2), iDurability, iMaxDurability, pItem->Type);
                    if (iGoldCost <= CharacterMachine->Gold)
                    {
                        SocketClient->ToGameServer()->SendRepairItemRequest(i, 1);
                    }
                }
            }
        }

        return 1;
    }

    int CMuHelper::Attack()
    {
        if (m_iCurrentTarget == -1)
        {
            if (!m_setTargets.empty())
            {
                CleanupTargets();

                if (m_config.bLongRangeCounterAttack)
                {
                    m_iCurrentTarget = GetFarthestAttackingTarget();
                }
                
                if (m_iCurrentTarget == -1)
                {
                    m_iCurrentTarget = GetNearestTarget();
                }
            }
            else
            {
                m_iComboState = 0;
                return 0;
            }
        }

        // Without any pickup the helper has no reason to leave the spot it was started on: it stands there,
        // attacks what is within reach and waits for the rest, instead of walking to monsters and back
        // (ghinzuro, 2026-10-22). With a pickup on it walks to its targets as before.
        const bool bStandStill = !PicksUpItems();

        // A skill the character can't cast now (too little energy for it, mana or AG) gives way to plain hits,
        // whether or not the plain attack is ticked as a fallback, and the chat says why, once (2026-10-23).
        // Without it the helper stood among the monsters doing nothing.
        bool bPlainAttack = m_config.bFallbackBasicAttack;
        const auto UsableOrPlain = [&](ActionSkillType iSkill)
        {
            const SkillBlock block = GetSkillBlock(iSkill);
            if (block == SkillBlock::None)
            {
                return true;
            }
            if (block == SkillBlock::Mana)
            {
                m_bManaShort = true;
            }
            NotifySkillBlocked(iSkill, block, true);
            bPlainAttack = true;
            return false;
        };

        if (m_config.bUseCombo)
        {
            if (UsableOrPlain((ActionSkillType)m_config.aiSkill[m_iComboState]))
            {
                if (bStandStill
                    && !MayAttackStandingStill(gSkillManager.GetSkillDistance((ActionSkillType)m_config.aiSkill[m_iComboState], Hero)))
                {
                    return 0;
                }
                return SimulateComboAttack();
            }
        }
        else
        {
            m_iCurrentSkill = SelectAttackSkill();
            if (m_iCurrentSkill > AT_SKILL_UNDEFINED && UsableOrPlain(m_iCurrentSkill))
            {
                const float fSkillDistance = gSkillManager.GetSkillDistance(m_iCurrentSkill, Hero);
                if (GameLogic::Combat::CanExecuteSkill(Hero, m_iCurrentSkill, fSkillDistance))
                {
                    if (bStandStill && !MayAttackStandingStill(fSkillDistance))
                    {
                        return 0;
                    }
                    const int iCast = SimulateAttack(m_iCurrentSkill);
                    if (iCast && m_iTimedSkillSlot >= 0 && m_iTimedSkillSlot < MaxSkillSlots)
                    {
                        m_aiTimedSkillSecond[m_iTimedSkillSlot] = m_iSecondsElapsed;
                    }
                    return iCast;
                }
            }
        }

        if (bPlainAttack)
        {
            if (!Hero->Movement)
            {
                if (bStandStill && !MayAttackStandingStill(GameLogic::Automation::EquippedAttackRange()))
                {
                    return 0;
                }
                return SimulateBasicAttack(m_iCurrentTarget);
            }
        }

        return 1;
    }

    bool CMuHelper::PicksUpItems() const
    {
        return m_config.bPickAllItems || m_config.bPickZen || m_config.bPickJewel || m_config.bPickAncient
            || m_config.bPickExcellent || m_config.bPickExtraItems;
    }

    bool CMuHelper::IsWithinReach(int iTargetId, float fReach) const
    {
        CHARACTER* pTarget = FindCharacterByKey(iTargetId);
        if (pTarget == nullptr || pTarget->Dead > 0)
        {
            return false;
        }

        // As CheckTile measures it: from the character to the middle of the target's tile.
        const float fTileX = static_cast<float>(static_cast<int>(pTarget->Object.Position[0] / TERRAIN_SCALE)) * TERRAIN_SCALE + TERRAIN_SCALE * 0.5f;
        const float fTileY = static_cast<float>(static_cast<int>(pTarget->Object.Position[1] / TERRAIN_SCALE)) * TERRAIN_SCALE + TERRAIN_SCALE * 0.5f;
        const float fDx = Hero->Object.Position[0] - fTileX;
        const float fDy = Hero->Object.Position[1] - fTileY;
        return fDx * fDx + fDy * fDy <= (fReach * TERRAIN_SCALE) * (fReach * TERRAIN_SCALE);
    }

    bool CMuHelper::MayAttackStandingStill(float fReach)
    {
        // Standing still, but not idle (ghinzuro, 2026-10-23): in the Arena the helper stood for seconds at a
        // time while monsters waited 4 to 7 tiles away, some behind the fence, out of the skill's reach. It still
        // attacks what is within reach from where it stands; only when nothing has been for StandStillPatience
        // does it go for the nearest monster within the hunting range of its start spot. With "original
        // position" ticked it walks back there as before (Regroup).
        if (KeepTargetWithinReach(fReach))
        {
            m_noTargetInReachSince = {};
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (m_noTargetInReachSince == std::chrono::steady_clock::time_point{})
        {
            m_noTargetInReachSince = now;
            return false;
        }
        if (now - m_noTargetInReachSince < StandStillPatience)
        {
            return false;
        }

        const int iTarget = GetNearestTargetNearSpot();
        if (iTarget == -1)
        {
            return false;
        }
        m_iCurrentTarget = iTarget;
        return true;
    }

    int CMuHelper::GetNearestTargetNearSpot()
    {
        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargets;
            _targetsLock.unlock();
        }

        int iNearest = -1;
        int iNearestDistance = 0;
        for (const int& iMonsterId : setTargets)
        {
            CHARACTER* pTarget = FindCharacterByKey(iMonsterId);
            if (pTarget == nullptr || pTarget->Dead > 0 || !pTarget->Object.Live || !IsMonster(pTarget))
            {
                continue;
            }
            if (ComputeDistanceBetween({ pTarget->PositionX, pTarget->PositionY }, m_posOriginal) > m_iHuntingDistance)
            {
                continue;
            }
            const int iDistance = ComputeDistanceFromTarget(pTarget);
            if (iNearest == -1 || iDistance < iNearestDistance)
            {
                iNearest = iMonsterId;
                iNearestDistance = iDistance;
            }
        }
        return iNearest;
    }

    bool CMuHelper::KeepTargetWithinReach(float fReach)
    {
        if (m_iCurrentTarget != -1 && IsWithinReach(m_iCurrentTarget, fReach))
        {
            return true;
        }

        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargets;
            _targetsLock.unlock();
        }

        int iNearest = -1;
        float fNearest = 0.f;
        for (const int& iMonsterId : setTargets)
        {
            CHARACTER* pTarget = FindCharacterByKey(iMonsterId);
            if (pTarget == nullptr || !IsMonster(pTarget) || !IsWithinReach(iMonsterId, fReach))
            {
                continue;
            }

            const float fDx = Hero->Object.Position[0] - pTarget->Object.Position[0];
            const float fDy = Hero->Object.Position[1] - pTarget->Object.Position[1];
            const float fDistance = fDx * fDx + fDy * fDy;
            if (iNearest == -1 || fDistance < fNearest)
            {
                iNearest = iMonsterId;
                fNearest = fDistance;
            }
        }

        if (iNearest == -1)
        {
            return false;
        }

        m_iCurrentTarget = iNearest;
        return true;
    }

    ActionSkillType CMuHelper::SelectAttackSkill()
    {
        m_iTimedSkillSlot = -1;
        const size_t safeSize = std::min({m_config.aiSkill.size(), m_config.aiSkillCondition.size(), m_config.aiSkillInterval.size()});
        for (int i = 1; i < (int)safeSize; i++)
        {
            const int iSkillId = m_config.aiSkill[i];
            if (iSkillId <= 0 || iSkillId >= MAX_SKILLS)
            {
                continue;
            }

            if ((m_config.aiSkillCondition[i] & ON_TIMER)
                && m_config.aiSkillInterval[i] != 0
                && m_iSecondsElapsed > 0
                && m_iSecondsElapsed % m_config.aiSkillInterval[i] == 0
                && (i >= MaxSkillSlots || m_aiTimedSkillSecond[i] != m_iSecondsElapsed))
            {
                m_iTimedSkillSlot = i;
                return (ActionSkillType)iSkillId;
            }

            if (m_config.aiSkillCondition[i] & ON_CONDITION)
            {
                int iCount = 0;
                if (m_config.aiSkillCondition[i] & ON_MOBS_NEARBY)
                {
                    iCount = (int)m_setTargets.size();
                }
                else if (m_config.aiSkillCondition[i] & ON_MOBS_ATTACKING)
                {
                    iCount = (int)m_setTargetsAttacking.size();
                }
                else
                {
                    continue;
                }

                if (((m_config.aiSkillCondition[i] & ON_MORE_THAN_TWO_MOBS)   && iCount >= 2)
                    || ((m_config.aiSkillCondition[i] & ON_MORE_THAN_THREE_MOBS) && iCount >= 3)
                    || ((m_config.aiSkillCondition[i] & ON_MORE_THAN_FOUR_MOBS)  && iCount >= 4)
                    || ((m_config.aiSkillCondition[i] & ON_MORE_THAN_FIVE_MOBS)  && iCount >= 5))
                {
                    return (ActionSkillType)iSkillId;
                }
            }
        }

        if (m_config.aiSkill[0] > 0)
        {
            return (ActionSkillType)m_config.aiSkill[0];
        }

        return AT_SKILL_UNDEFINED;
    }

    int CMuHelper::SimulateComboAttack()
    {
        for (int i = 0; i < m_config.aiSkill.size(); i++)
        {
            if (m_config.aiSkill[i] == 0)
            {
                return 0;
            }
        }

        if (SimulateAttack((ActionSkillType)m_config.aiSkill[m_iComboState]))
        {
            m_iComboState = (m_iComboState + 1) % 3;
        }

        return 1;
    }

    int CMuHelper::SimulateAttack(ActionSkillType iSkill)
    {
        return SimulateSkill(iSkill, true, m_iCurrentTarget);
    }

    int CMuHelper::SimulateSkill(ActionSkillType iSkill, bool bTargetRequired, int iTarget)
    {
        using GameLogic::Automation::SkillResult;

        // Players are allowed here, and must be: the helper casts its buffs and
        // heals on party members and on the hero itself. Only the plain attack
        // is monster-only, as it always was. A target that is not attackable is
        // still refused by the primitive when it is dead.
        const SkillResult result =
            GameLogic::Automation::CastSkill(iSkill, bTargetRequired, iTarget, true, m_iHuntingDistance);
        m_lastSkillResult = result;

        if (iTarget != -1)
        {
            ForgetTarget(iTarget, result);
        }

        return result == SkillResult::Cast ? 1 : 0;
    }

    int CMuHelper::SimulateBasicAttack(int iTarget)
    {
        using GameLogic::Automation::AttackResult;

        // No target is not a failed attack: without this the helper asks the
        // automation for object -1 and then forgets a target it never had.
        if (iTarget == -1)
        {
            return 0;
        }

        const AttackResult result = GameLogic::Automation::AttackObject(iTarget, false, m_iHuntingDistance);

        if (GameLogic::Automation::ShouldForgetTarget(result))
        {
            DeleteTarget(iTarget);
            m_mapForgottenUntil[iTarget] = std::chrono::steady_clock::now() + ForgottenTargetRest;
        }

        return result == AttackResult::Attacked ? 1 : 0;
    }

    int CMuHelper::Regroup()
    {
        if (m_config.bReturnToOriginalPosition && m_iSecondsAway > m_config.iMaxSecondsAway)
        {
            // Let the current swing finish first: a walk started mid-swing cuts the attack
            // animation, and what a swing fires late in it (the arrows of Triple Shot, for one)
            // never appears. Attacking and picking up already wait for the swing.
            if (GameLogic::Automation::IsSwingInProgress())
            {
                return 0;
            }

            if (!SimulateMove(m_posOriginal))
            {
                return 0;
            }

            m_iSecondsAway = 0;
            m_iComboState = 0;
            m_iCurrentTarget = -1;
        }

        return 1;
    }

    int CMuHelper::SimulateMove(POINT posMove)
    {
        const GameLogic::Automation::MoveResult result =
            GameLogic::Automation::WalkTo(static_cast<int>(posMove.x), static_cast<int>(posMove.y));

        return result == GameLogic::Automation::MoveResult::Arrived ? 1 : 0;
    }

    bool CMuHelper::HasAssignedBuffSkill()
    {
        for (int i = 0; i < m_config.aiBuff.size(); i++)
        {
            if (m_config.aiBuff[i] != 0)
            {
                return true;
            }
        }

        return false;
    }

    ActionSkillType CMuHelper::GetHealingSkill()
    {
        std::vector<ActionSkillType> aiHealingSkills =
        {
            AT_SKILL_HEALING,
            AT_SKILL_HEALING_STR,
        };

        for (int i = 0; i < aiHealingSkills.size(); i++)
        {
            int iSkillIndex = g_pSkillList->GetSkillIndex(aiHealingSkills[i]);
            if (iSkillIndex != -1)
            {
                return aiHealingSkills[i];
            }
        }

        return AT_SKILL_UNDEFINED;
    }

    bool CMuHelper::IsSelfPositionSkill(ActionSkillType iSkill)
    {
        return GameLogic::Automation::IsSelfPositionSkill(iSkill);
    }

    ActionSkillType CMuHelper::GetDrainLifeSkill()
    {
        std::vector<ActionSkillType> aiDrainLifeSkills =
        {
            AT_SKILL_ALICE_DRAINLIFE,
            AT_SKILL_ALICE_DRAINLIFE_STR
        };

        for (int i = 0; i < aiDrainLifeSkills.size(); i++)
        {
            int iSkillIndex = g_pSkillList->GetSkillIndex(aiDrainLifeSkills[i]);
            if (iSkillIndex != -1)
            {
                return aiDrainLifeSkills[i];
            }
        }

        return AT_SKILL_UNDEFINED;
    }

    int CMuHelper::ObtainItem()
    {
        if (m_iCurrentItem == MAX_ITEMS)
        {
            m_iCurrentItem = SelectItemToObtain();
            if (m_iCurrentItem == MAX_ITEMS)
            {
                return 1;
            }
        }

        // The walk to a drop would cut the current swing short, like the walk back in Regroup:
        // finish the attack first, then go for the drop.
        if (GameLogic::Automation::IsSwingInProgress())
        {
            return 0;
        }

        using GameLogic::Automation::PickupResult;
        const PickupResult result = GameLogic::Automation::PickUpItem(m_iCurrentItem, m_iObtainingDistance);

        switch (result)
        {
        case PickupResult::Requested:
            // The drop stays tracked until it leaves the ground (DeleteItem from the viewport): the
            // server refuses a pickup while it still sees the character a few tiles away, for example
            // after a quick walk between many drops, and a refused drop must be tried again rather than
            // left lying there. A drop refused MAX_PICKUP_ATTEMPTS times is given up.
            if (CountPickupAttempt(m_iCurrentItem) >= MAX_PICKUP_ATTEMPTS)
            {
                DeleteItem(m_iCurrentItem);
            }

            m_iCurrentItem = MAX_ITEMS;
            return 1;
        case PickupResult::Gone:
            DeleteItem(m_iCurrentItem);
            return 1;
        case PickupResult::Approaching:
        case PickupResult::NoPath:
            // Walking towards it: the helper's other work waits a frame.
            return 0;
        case PickupResult::TooFar:
            // Out of reach now (the character moved on to fight): choose the closest drop again on the
            // next tick instead of waiting for this one to expire, which kept every other drop waiting.
            m_iCurrentItem = MAX_ITEMS;
            return 1;
        case PickupResult::Busy:
            return 1;
        }

        return 1;
    }

    bool CMuHelper::ShouldObtainItem(int iItemId)
    {
        ITEM_t* pDrop = &Items[iItemId];
        ITEM* pItem = &pDrop->Item;

        if ((m_config.bPickZen && IsMoneyItem(pItem))
            || (m_config.bPickJewel && GameLogic::Items::IsJewelItem(pItem))
            || (m_config.bPickAncient && IsAncientItem(pItem))
            || (m_config.bPickExcellent && IsExcellentItem(pItem)))
        {
            return true;
        }

        if (m_config.bPickExtraItems)
        {
            std::wstring strDisplayName = GetItemDisplayName(pItem);

            for (const auto& str : m_config.aExtraItems)
            {
                // Check if the search keyword is in the item's display name
                if (strDisplayName.find(str) != std::wstring::npos)
                {
                    return true;
                }
            }
        }

        return m_config.bPickAllItems;
    }

    void CMuHelper::AddItem(int iItemId, POINT posWhere)
    {
        _itemsLock.lock();
        m_setItems.insert(iItemId);
        m_mapPickupAttempts.erase(iItemId); // a new drop may reuse the id of an old one
        _itemsLock.unlock();
    }

    void CMuHelper::DeleteItem(int iItemId)
    {
        _itemsLock.lock();
        m_setItems.erase(iItemId);
        m_mapPickupAttempts.erase(iItemId);
        _itemsLock.unlock();

        if (iItemId == m_iCurrentItem)
        {
            m_iCurrentItem = MAX_ITEMS;
        }
    }

    int CMuHelper::CountPickupAttempt(int iItemId)
    {
        _itemsLock.lock();
        const int iAttempts = ++m_mapPickupAttempts[iItemId];
        _itemsLock.unlock();
        return iAttempts;
    }

    int CMuHelper::SelectItemToObtain()
    {
        int iClosestItemId = MAX_ITEMS;
        int iMinDistance = m_config.iObtainingRange;

        std::set<int> setItems;
        {
            _itemsLock.lock();
            setItems = m_setItems;
            _itemsLock.unlock();
        }

        for (const int& iItemId : setItems)
        {
            if (!ShouldObtainItem(iItemId))
            {
                continue;
            }

            int iItemX = (int)(Items[iItemId].Object.Position[0] / TERRAIN_SCALE);
            int iItemY = (int)(Items[iItemId].Object.Position[1] / TERRAIN_SCALE);

            int iDistance = ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, { iItemX, iItemY });
            if (iDistance <= iMinDistance)
            {
                iMinDistance = iDistance;
                iClosestItemId = iItemId;
            }
        }

        return iClosestItemId;
    }
}
