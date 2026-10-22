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
		void AddCost(int iCost) { m_iTotalCost += iCost; }
		int GetTotalCost() { return m_iTotalCost; }

		void AddTarget(int iTargetId, bool bIsAttacking);
		void DeleteTarget(int iTargetId);
		void DeleteAllTargets();

		void AddItem(int iItemId, POINT posDropped);
		void DeleteItem(int iItemId);

	private:
		void WorkLoop(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
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

		// The second (m_iSecondsElapsed) in which each skill slot last went off on its timer: once per due
		// second, now that the attack runs more often than four times a second.
		static constexpr int MaxSkillSlots = 8;
		std::array<int, MaxSkillSlots> m_aiTimedSkillSecond{};
		int m_iTimedSkillSlot = -1;
	};

	extern CMuHelper g_MuHelper;
}