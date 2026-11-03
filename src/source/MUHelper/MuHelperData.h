#pragma once

#include <cstdint>
#include <set>
#include <array>
#include <string>
#include "Network/Server/WSclient.h"

namespace MUHelper
{
	enum ESkillActivationBase : uint32_t
	{
		ALWAYS = 0x00000000,
		ON_TIMER = 0x00000001,
		ON_CONDITION = 0x00000002,
	};

	enum ESkillActivationPreCon : uint32_t
	{
		ON_MOBS_NEARBY = 0x00000004,
		ON_MOBS_ATTACKING = 0x00000008
	};

	enum ESkillActivationSubCon : uint32_t
	{
		ON_MORE_THAN_TWO_MOBS = 0x00000010,
		ON_MORE_THAN_THREE_MOBS = 0x00000020,
		ON_MORE_THAN_FOUR_MOBS = 0x00000040,
		ON_MORE_THAN_FIVE_MOBS = 0x00000080
	};

	DEFINE_ENUM_FLAG_OPERATORS(ESkillActivationBase);
	DEFINE_ENUM_FLAG_OPERATORS(ESkillActivationPreCon);
	DEFINE_ENUM_FLAG_OPERATORS(ESkillActivationSubCon);

	constexpr uint32_t MUHELPER_SKILL_PRECON_CLEAR =
		~(static_cast<uint32_t>(ON_MOBS_NEARBY) | 
			static_cast<uint32_t>(ON_MOBS_ATTACKING));

	constexpr uint32_t MUHELPER_SKILL_SUBCON_CLEAR =
		~(static_cast<uint32_t>(ON_MORE_THAN_TWO_MOBS) | 
			static_cast<uint32_t>(ON_MORE_THAN_THREE_MOBS) |
			static_cast<uint32_t>(ON_MORE_THAN_FOUR_MOBS) | 
			static_cast<uint32_t>(ON_MORE_THAN_FIVE_MOBS));

	enum EPetAttackMode : BYTE
	{
		PET_ATTACK_CEASE = 0x00,
		PET_ATTACK_AUTO = 0x01,
		PET_ATTACK_TOGETHER = 0x02
	};

	typedef struct _PetAttackConfig
	{
		int iHuntingRange = 0;

		bool bLongRangeCounterAttack = false;
		bool bReturnToOriginalPosition = false;
		int iMaxSecondsAway = 0;

		std::array<uint32_t, 3> aiSkill = { 0, 0, 0 };
		std::array<uint32_t, 3> aiSkillCondition = { 0, 0, 0 };
		std::array<uint32_t, 3> aiSkillInterval = { 0, 0, 0 };

		bool bUseCombo = false;

		std::array<uint32_t, 3> aiBuff = { 0, 0, 0 };

		bool bBuffDuration = false;
		bool bBuffDurationParty = false;
		int iBuffCastInterval = 0;

		bool bAutoHeal = false;
		int iHealThreshold = 0;
		bool bSupportParty = false;
		bool bAutoHealParty = false;
		int iHealPartyThreshold = 0;

		bool bUseHealPotion = false;
		int iPotionThreshold = 0;
		// Mana potions below this percent of mana (2026-11-02, ghinzuro: the DK stopped drinking them).
		int iManaPotionThreshold = DefaultManaPotionThreshold;
		static constexpr int DefaultManaPotionThreshold = 30;

		bool bUseDrainLife = false;
		bool bUseDarkRaven = false;
		int iDarkRavenMode = 0;

		bool bRepairItem = false;

		int iObtainingRange = 0;
		bool bPickAllItems = false;
		bool bPickSelectItems = false;
		bool bPickJewel = false;
		bool bPickZen = false;
		bool bPickAncient = false;
		bool bPickExcellent = false;
		bool bPickContainers = false; // saved in byte 33 bit 4; 0 in blobs from older clients = off
		bool bPickQuestItems = false; // saved in byte 33 bit 5; 0 in blobs from older clients = off
		bool bPickExtraItems = false;
		std::set<std::wstring> aExtraItems;

		// Client-local settings (byte index 33, bits 0-3).
		// The server (OpenMU) stores MuHelperConfiguration as byte[] echoed
		// unchanged. It does not parse or enforce these bits. Adding new bits
		// in unused positions is safe. However, changing existing bits will
		// break offline leveling bots, since the server side bot relies on the
		// same packet layout.
		bool bUseSelfDefense = false;
		bool bAutoAcceptFriend = false;
		bool bAutoAcceptGuild = false;
		bool bFallbackBasicAttack = true;
	} ConfigData;

	// The saved settings hold 12 extra item names of 15 bytes (UTF-8) each.
	constexpr size_t MaxExtraItems = 12;
	constexpr size_t MaxExtraItemBytes = 15;

	// The name as it will be saved: spaces trimmed, cut to MaxExtraItemBytes on a character boundary.
	// Names are matched as a part of the item name, so a cut name still picks the item up.
	std::wstring FitExtraItemName(const std::wstring& name);

	class ConfigDataSerDe {
	public:
		static void Serialize(const ConfigData& domain, PRECEIVE_MUHELPER_DATA& packet);
		static void Deserialize(const PRECEIVE_MUHELPER_DATA& packet, ConfigData& domain);
	};
}
