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
#include "GameLogic/Automation/Attack.h"
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

		void AddTarget(int iTargetId, bool bIsAttacking, bool bAttacksHero = false);
		void DeleteTarget(int iTargetId);
		void DeleteAllTargets();

		void AddItem(int iItemId, POINT posDropped);
		void DeleteItem(int iItemId);

	private:
		void WorkLoop(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
		void StopStuckWalk(std::chrono::steady_clock::time_point now);
		// Starts the fight over when the character stood among monsters without hitting for FightStallLimit.
		void UnstickFight(std::chrono::steady_clock::time_point now);
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
		// Whether a live monster among the targets is within the attack's reach, at most FightReachLimit tiles.
		bool IsMonsterWithinAttackReach() const;
		int RepairEquipments();
		int Regroup();
		ActionSkillType SelectAttackSkill();
		int SimulateAttack(ActionSkillType iSkill);
		int SimulateSkill(ActionSkillType iSkill, bool bTargetRequired, int iTarget);
		int SimulateBasicAttack(int iTarget);
		int SimulateComboAttack();
		int GetNearestTarget();
		int GetFarthestAttackingTarget();
		// "Strong monsters first": a boss or a much stronger monster among the targets (the server's /mst numbers,
		// GameLogic/Automation/TargetPriority.h), within the hunting range of the hero, or of the start spot when
		// standing still; -1 when there is none. Remembered in m_iStrongTarget.
		int UpdateStrongTarget(bool bNearSpot);
		bool IsFightingStrongTargetNearSpot();
		void CleanupTargets();
		int ComputeDistanceByRange(int iRange);
		int ComputeDistanceFromTarget(CHARACTER* pTarget);
		int ComputeDistanceBetween(POINT posA, POINT posB);
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
		std::set<int> m_setAttackersOfHero;
		std::set<int> m_setItems;
		std::map<int, int> m_mapPickupAttempts; // drop id -> pickups requested; guarded like m_setItems
		int m_iCurrentItem;
		int m_iCurrentTarget;
		int m_iStrongTarget = -1; // the strong target UpdateStrongTarget picked, -1 = none
		int m_iCurrentBuffIndex;
		int m_iCurrentBuffPartyIndex;
		int m_iBuffWaitRounds = 0;                                          // rounds the current member's buff waited
		static constexpr int BuffWaitRoundLimit = 8;                        // rounds, about 2 seconds
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
		std::map<int, std::chrono::steady_clock::time_point> m_mapHealSkipUntil;  // party member key -> skipped by the heal until
		static constexpr std::chrono::seconds HealSkipTime{ 3 };
		GameLogic::Automation::SkillResult m_lastSkillResult = GameLogic::Automation::SkillResult::NotReady;
		bool m_bManaShort = false;
		bool m_bLastPotionWasMana = false;                                  // alternates when both are needed
		std::set<int> m_setNotified;                                         // chat notices given this run
		static constexpr std::chrono::milliseconds StandStillPatience{ 1000 };
		static constexpr std::chrono::milliseconds StuckWalkLimit{ 1200 };
		// The last hit or cast that went out; a standing character with a monster close by and none for
		// FightStallLimit is started over (UnstickFight, 2026-11-06).
		static constexpr std::chrono::milliseconds FightStallLimit{ 4000 };
		std::chrono::steady_clock::time_point m_lastHitAt{};
		bool m_bStallLogged = false; // the stall since the last hit is in MuError.log
		const char* m_pszRoundStop = ""; // the step the last round got to (for the stall log)
		GameLogic::Automation::AttackResult m_lastAttackResult = GameLogic::Automation::AttackResult::NoTarget;
		std::chrono::steady_clock::time_point m_walkWatchSince{};
		POINT m_posWalkWatch{};
		std::chrono::steady_clock::time_point m_noTargetInReachSince{};
		// The walk back to the start spot (Regroup) is put off for RegroupRest when there is no way there or it
		// gets no closer (WalkGap) for RegroupPatience, as with monsters all round (2026-11-03); the helper fights meanwhile.
		static constexpr std::chrono::milliseconds RegroupPatience{ 2000 };
		static constexpr std::chrono::seconds RegroupRest{ 3 };
		std::chrono::steady_clock::time_point m_regroupSince{};
		std::chrono::steady_clock::time_point m_regroupRestUntil{};
		int m_iRegroupBestGap = 0; // the smallest WalkGap back seen in this attempt
		// A drop with no way to it, or whose walk gets no closer (WalkGap) for PickupPatience (monsters on and round
		// it), is set aside for PickupRest and the round goes on to the attack (ShouldSetAsidePickup, 2026-11-06).
		// After such a walk the whole pickup rests for PickupStuckRest, so drop after drop can't keep the attack waiting.
		static constexpr std::chrono::milliseconds PickupPatience{ 2000 };
		static constexpr std::chrono::seconds PickupRest{ 5 };
		static constexpr std::chrono::seconds PickupStuckRest{ 3 };
		std::chrono::steady_clock::time_point m_pickupSince{};
		std::chrono::steady_clock::time_point m_pickupRestUntil{};
		int m_iPickupBestGap = 0; // the smallest WalkGap to the current drop seen in this attempt
		// While a monster is within the attack's reach, a drop that takes a walk waits (ShouldPutOffPickup), at most
		// PickupFightWait; then one walk is tried (m_bPickupFightTry) with the patience above (2026-11-06). In a fight
		// that never ends that is one far drop per PickupFightWait, on purpose: the attack comes first.
		static constexpr std::chrono::seconds PickupFightWait{ 10 };
		static constexpr float FightReachLimit = 3.0f; // tiles: a monster further away doesn't put a drop off
		std::chrono::steady_clock::time_point m_pickupPutOffSince{};
		bool m_bPickupFightTry = false;
		bool m_bPickupWalking = false; // the last pickup step started or went on with a walk to the drop
		std::map<int, std::chrono::steady_clock::time_point> m_mapItemRestUntil; // drop id -> back in the choice; guarded like m_setItems
		void SetAsideItem(int iItemId, std::chrono::steady_clock::time_point now);
		static constexpr std::chrono::seconds ForgottenTargetRest{ 2 };
		std::map<int, std::chrono::steady_clock::time_point> m_mapForgottenUntil; // monster key -> back in the list
	};

	extern CMuHelper g_MuHelper;
}