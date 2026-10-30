#pragma once

#include <functional>
#include <array>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <atomic>

#include "MuHelperData.h"
#include "GameLogic/Automation/Skill.h"

namespace MUHelper
{
	class CMuHelper
	{
	public:
		CMuHelper() = default;
		~CMuHelper() = default;

	public:
		static void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

		ConfigData GetConfig() const;
		void Save(const ConfigData& config);
		void Load(const ConfigData& config);
		void Start();
		void Stop();
		void Toggle();
		void TriggerStart();
		void TriggerStop();
		bool IsActive() { return m_bActive; }
		// The player walks the character by hand (the phone's joystick, the pad's stick) while the helper
		// runs: meanwhile it only drinks its potions, as the stick and the helper pulled the character
		// two ways; afterwards it hunts around where the walk ended, and goes back there, not to where
		// it started.
		void SetSteered(bool bSteered);
		void AddCost(int iCost) { m_iTotalCost += iCost; }
		int GetTotalCost() { return m_iTotalCost; }

		void AddTarget(int iTargetId, bool bIsAttacking);
		void DeleteTarget(int iTargetId);
		void DeleteAllTargets();

		void AddItem(int iItemId, POINT posDropped);
		void DeleteItem(int iItemId);

	private:
		void WorkLoop(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
		void StopStuckWalk(std::chrono::steady_clock::time_point now);
		void Work();
		int ActivatePet();
		int Buff();
		int BuffTarget(CHARACTER* pTargetChar, ActionSkillType iBuffSkill);
		int RecoverHealth();
		int Heal();
		int HealSelf(ActionSkillType iHealingSkill);
		int DrainLife();
		int ConsumePotion();
		int Attack();
		// Whether any pickup is switched on (all items, zen, jewels, ancients, excellents or the extra list).
		bool PicksUpItems() const;
		// Standing still: keeps a target within the reach (tiles) of the next attack, switching to the nearest
		// one that is; false when none is.
		bool KeepTargetWithinReach(float fReach);
		// Standing still but not idle: true when a target is within the reach, or when none has been for
		// StandStillPatience and a target stands within the hunting range of the start spot (then the current one).
		bool MayAttackStandingStill(float fReach);
		int GetNearestTargetNearSpot();
		bool IsWithinReach(int iTargetId, float fReach) const;
		int RepairEquipments();
		int Regroup();
		ActionSkillType SelectAttackSkill();
		int SimulateAttack(ActionSkillType iSkill);
		int SimulateSkill(ActionSkillType iSkill, bool bTargetRequired, int iTarget);
		int SimulateBasicAttack(int iTarget);
		int SimulateComboAttack();
		int GetNearestTarget();
		int GetFarthestAttackingTarget();
		void CleanupTargets();
		int ComputeDistanceByRange(int iRange);
		int ComputeDistanceFromTarget(CHARACTER* pTarget);
		int ComputeDistanceBetween(POINT posA, POINT posB);
		int SimulateMove(POINT posMove);
		int ObtainItem();
		int CountPickupAttempt(int iItemId);
		int SelectItemToObtain();
		bool ShouldObtainItem(int iItemId);
		ActionSkillType GetHealingSkill();
		ActionSkillType GetDrainLifeSkill();
		bool HasAssignedBuffSkill();
		bool IsSelfPositionSkill(ActionSkillType iSkill);

		// What keeps the character from casting a skill right now, by the client's own checks: the skill's energy
		// (with the gear's bonus), its leadership (Dark Lord: Critical Damage 300, Fire Scream 70), its other stat
		// requirements, its mana or its AG.
		enum class SkillBlock { None, Energy, Mana, Ability, Leadership, Requirements };
		SkillBlock GetSkillBlock(ActionSkillType iSkill) const;
		// A buff or heal: cast it, or pass it over when it can't be cast, so the round goes on to the potions
		// and the attack. 0 only while it is worth waiting for (a swing playing, a walk towards the target);
		// a cast the client refused is passed over too and tried again on the next round.
		int CastSupport(ActionSkillType iSkill, bool bTargetRequired, int iTarget);
		bool IsSkillSuspended(ActionSkillType iSkill) const;
		// Every round: the live monsters in view within the hunting range become targets, not only those the
		// server reported moving, appearing or attacking (2026-10-23).
		void RefreshTargets();
		// A target given up for itself (out of view, dead, behind a wall, no path) rests for a moment.
		void ForgetTarget(int iTargetId, GameLogic::Automation::SkillResult result);
		// Says once per run in the chat why a skill isn't used.
		void NotifySkillBlocked(ActionSkillType iSkill, SkillBlock block, bool bPlainAttack);
		void NotifyOnce(int iKey, const wchar_t* pszText);

	private:
		ConfigData m_config;
		POINT m_posOriginal;
		std::thread m_timerThread;
		std::atomic<bool> m_bActive;
		std::set<int> m_setTargets;
		std::set<int> m_setTargetsAttacking;
		std::set<int> m_setItems;
		std::map<int, int> m_mapPickupAttempts; // drop id -> pickups requested; guarded like m_setItems
		int m_iCurrentItem;
		int m_iCurrentTarget;
		int m_iCurrentBuffIndex;
		int m_iCurrentBuffPartyIndex;
		int m_iCurrentHealPartyIndex;
		int m_iComboState;
		ActionSkillType m_iCurrentSkill;
		int m_iHuntingDistance;
		int m_iObtainingDistance;
		int m_iLoopCounter;
		int m_iSecondsElapsed;
		int m_iSecondsAway;
		bool m_bTimerActivatedBuffOngoing;
		bool m_bPetActivated;
		int m_iTotalCost;

		// The whole round runs every RoundInterval; between rounds, on every frame, only the attack, while the
		// last round got as far as it (WorkLoop).
		static constexpr std::chrono::milliseconds RoundInterval{ 250 };
		std::chrono::steady_clock::time_point m_nextRound{};
		bool m_bAttackBetweenRounds = false;
		bool m_bSteered = false;                                             // SetSteered

		// The second (m_iSecondsElapsed) in which each skill slot last went off on its timer: once per due
		// second, now that the attack runs more often than four times a second.
		static constexpr int MaxSkillSlots = 8;
		std::array<int, MaxSkillSlots> m_aiTimedSkillSecond{};
		int m_iTimedSkillSlot = -1;

		// Buffs and heals that can't be cast don't hold up the round any more (2026-10-23), nor one the client
		// refuses for a reason the helper doesn't check itself (2026-10-24). One that keeps failing rests a while.
		static constexpr int SupportFailureLimit = 12;                      // rounds, about 3 seconds
		static constexpr std::chrono::seconds SupportSuspendTime{ 10 };
		std::map<int, int> m_mapSupportFailures;                             // skill -> failed rounds in a row
		std::map<int, std::chrono::steady_clock::time_point> m_mapSuspendedUntil;
		GameLogic::Automation::SkillResult m_lastSkillResult = GameLogic::Automation::SkillResult::NotReady;
		// Mana potions: below ManaPotionPercent, or when a skill found too little mana since the last round.
		static constexpr int ManaPotionPercent = 30;
		bool m_bManaShort = false;
		std::set<int> m_setNotified;                                         // chat notices given this run
		static constexpr std::chrono::milliseconds StandStillPatience{ 1000 };
		static constexpr std::chrono::milliseconds StuckWalkLimit{ 1200 };
		std::chrono::steady_clock::time_point m_walkWatchSince{};
		POINT m_posWalkWatch{};
		std::chrono::steady_clock::time_point m_noTargetInReachSince{};
		static constexpr std::chrono::seconds ForgottenTargetRest{ 2 };
		std::map<int, std::chrono::steady_clock::time_point> m_mapForgottenUntil; // monster key -> back in the list
	};

	extern CMuHelper g_MuHelper;
}