#include "stdafx.h"
#include "MuHelperData.h"

#include <cstdlib>
#include <cstring>

#include "Core/Utilities/StringUtils.h"

namespace MUHelper
{
	// True when the bytes are well-formed UTF-8 (no overlong forms or surrogates).
	static bool IsValidUtf8(const std::string& bytes)
	{
		size_t i = 0;
		while (i < bytes.size())
		{
			const unsigned char c = static_cast<unsigned char>(bytes[i]);
			size_t extra = 0;
			unsigned int cp = 0;
			if (c < 0x80) { ++i; continue; }
			else if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
			else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
			else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
			else return false;
			if (i + extra >= bytes.size()) return false;
			for (size_t k = 1; k <= extra; ++k)
			{
				const unsigned char cc = static_cast<unsigned char>(bytes[i + k]);
				if ((cc & 0xC0) != 0x80) return false;
				cp = (cp << 6) | (cc & 0x3F);
			}
			const unsigned int minimum[] = { 0, 0x80, 0x800, 0x10000 };
			if (cp < minimum[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
			i += extra + 1;
		}
		return true;
	}

	std::wstring FitExtraItemName(const std::wstring& name)
	{
		const size_t first = name.find_first_not_of(L" \t");
		if (first == std::wstring::npos)
		{
			return {};
		}
		std::wstring fitted = name.substr(first, name.find_last_not_of(L" \t") - first + 1);
		while (!fitted.empty() && StringUtils::WideToNarrow(fitted.c_str()).size() > MaxExtraItemBytes)
		{
			fitted.pop_back();
		}
		while (!fitted.empty() && (fitted.back() == L' ' || fitted.back() == L'\t'))
		{
			fitted.pop_back();
		}
		return fitted;
	}

	void ConfigDataSerDe::Serialize(const ConfigData& gameData, PRECEIVE_MUHELPER_DATA& netData)
	{
		memset(&netData, 0, sizeof(netData));

		netData.HuntingRange = static_cast<BYTE>(gameData.iHuntingRange & 0x0F);
		netData.DistanceMin = static_cast<BYTE>(gameData.iMaxSecondsAway & 0x0F);
		netData.LongDistanceAttack = gameData.bLongRangeCounterAttack ? 1 : 0;
		netData.OriginalPosition = gameData.bReturnToOriginalPosition ? 1 : 0;

		netData.BasicSkill1 = static_cast<WORD>(gameData.aiSkill[0] & 0xFFFF);
		netData.ActivationSkill1 = static_cast<WORD>(gameData.aiSkill[1] & 0xFFFF);
		netData.ActivationSkill2 = static_cast<WORD>(gameData.aiSkill[2] & 0xFFFF);

		netData.DelayMinSkill1 = static_cast<WORD>(gameData.aiSkillInterval[1] & 0xFFFF);
		netData.DelayMinSkill2 = static_cast<WORD>(gameData.aiSkillInterval[2] & 0xFFFF);

		if (gameData.aiSkillCondition[1] & ON_TIMER)
		{
			netData.Skill1Delay = 1;
		}
		if (gameData.aiSkillCondition[1] & ON_CONDITION)
		{
			netData.Skill1Con = 1;
		}
		if (gameData.aiSkillCondition[1] & ON_MOBS_NEARBY)
		{
			netData.Skill1PreCon = 0;
		}
		else if (gameData.aiSkillCondition[1] & ON_MOBS_ATTACKING)
		{
			netData.Skill1PreCon = 1;
		}

		if (gameData.aiSkillCondition[1] & ON_MORE_THAN_TWO_MOBS)
		{
			netData.Skill1SubCon = 0;
		}
		else if (gameData.aiSkillCondition[1] & ON_MORE_THAN_THREE_MOBS)
		{
			netData.Skill1SubCon = 1;
		}
		else if (gameData.aiSkillCondition[1] & ON_MORE_THAN_FOUR_MOBS)
		{
			netData.Skill1SubCon = 2;
		}
		else if (gameData.aiSkillCondition[1] & ON_MORE_THAN_FIVE_MOBS)
		{
			netData.Skill1SubCon = 3;
		}

		if (gameData.aiSkillCondition[2] & ON_TIMER)
		{
			netData.Skill2Delay = 1;
		}
		if (gameData.aiSkillCondition[2] & ON_CONDITION)
		{
			netData.Skill2Con = 1;
		}
		if (gameData.aiSkillCondition[2] & ON_MOBS_NEARBY)
		{
			netData.Skill2PreCon = 0;
		}
		else if (gameData.aiSkillCondition[2] & ON_MOBS_ATTACKING)
		{
			netData.Skill2PreCon = 1;
		}

		if (gameData.aiSkillCondition[2] & ON_MORE_THAN_TWO_MOBS)
		{
			netData.Skill2SubCon = 0;
		}
		else if (gameData.aiSkillCondition[2] & ON_MORE_THAN_THREE_MOBS)
		{
			netData.Skill2SubCon = 1;
		}
		else if (gameData.aiSkillCondition[2] & ON_MORE_THAN_FOUR_MOBS)
		{
			netData.Skill2SubCon = 2;
		}
		else if (gameData.aiSkillCondition[2] & ON_MORE_THAN_FIVE_MOBS)
		{
			netData.Skill2SubCon = 3;
		}

		netData.Combo = gameData.bUseCombo ? 1 : 0;

		netData.BuffSkill0NumberID = static_cast<WORD>(gameData.aiBuff[0] & 0xFFFF);
		netData.BuffSkill1NumberID = static_cast<WORD>(gameData.aiBuff[1] & 0xFFFF);
		netData.BuffSkill2NumberID = static_cast<WORD>(gameData.aiBuff[2] & 0xFFFF);

		netData.BuffDuration = gameData.bBuffDuration ? 1 : 0;
		netData.BuffDurationforAllPartyMembers = gameData.bBuffDurationParty ? 1 : 0;
		netData.CastingBuffMin = static_cast<WORD>(gameData.iBuffCastInterval);

		netData.AutoHeal = gameData.bAutoHeal ? 1 : 0;
		netData.HPStatusAutoPotion = static_cast<BYTE>((gameData.iPotionThreshold / 10) & 0x0F);
		netData.MPStatusAutoPotion = static_cast<BYTE>((gameData.iManaPotionThreshold / 10 + 1) & 0x0F);
		netData.HPStatusAutoHeal = static_cast<BYTE>((gameData.iHealThreshold / 10) & 0x0F);
		netData.AutoPotion = gameData.bUseHealPotion ? 1 : 0;
		netData.DrainLife = gameData.bUseDrainLife ? 1 : 0;
		netData.Party = gameData.bSupportParty ? 1 : 0;
		netData.PreferenceOfPartyHeal = gameData.bAutoHealParty ? 1 : 0;

		netData.HPStatusOfPartyMembers = static_cast<BYTE>((gameData.iHealPartyThreshold / 10) & 0x0F);
		netData.HPStatusDrainLife = static_cast<BYTE>((gameData.iHealThreshold / 10) & 0x0F);

		netData.UseDarkSpirits = gameData.bUseDarkRaven ? 1 : 0;
		netData.PetAttack = static_cast<BYTE>(gameData.iDarkRavenMode);

		netData.RepairItem = gameData.bRepairItem ? 1 : 0;
		netData.ObtainRange = static_cast<BYTE>(gameData.iObtainingRange & 0x0F);
		netData.PickAllNearItems = gameData.bPickAllItems ? 1 : 0;
		netData.PickSelectedItems = gameData.bPickSelectItems ? 1 : 0;
		netData.Zen = gameData.bPickZen ? 1 : 0;
		netData.JewelOrGem = gameData.bPickJewel ? 1 : 0;
		netData.ExcellentItem = gameData.bPickExcellent ? 1 : 0;
		netData.SetItem = gameData.bPickAncient ? 1 : 0;
		netData.AddExtraItem = gameData.bPickExtraItems ? 1 : 0;

		// Names of 15 bytes or more used to be dropped here, so "Symbol of Kundun" was gone after a save
		// (ghinzuro, 2026-11-03). Now they are cut to fit.
		memset(netData.ExtraItems, 0, sizeof(netData.ExtraItems));
		size_t iItemIndex = 0;
		for (const auto& wsItem : gameData.aExtraItems)
		{
			if (iItemIndex >= MaxExtraItems)
			{
				break;
			}

			const std::string utf8 = StringUtils::WideToNarrow(FitExtraItemName(wsItem).c_str());
			if (utf8.empty())
			{
				continue;
			}
			memcpy(netData.ExtraItems[iItemIndex], utf8.data(), utf8.size());
			iItemIndex++;
		}

		// byte 33, client-local. server echoes unchanged, see MuHelperData.h
		netData.bUseSelfDefense = gameData.bUseSelfDefense ? 1 : 0;
		netData.bAutoAcceptFriend = gameData.bAutoAcceptFriend ? 1 : 0;
		netData.bAutoAcceptGuild = gameData.bAutoAcceptGuild ? 1 : 0;
		netData.bFallbackBasicAttack = gameData.bFallbackBasicAttack ? 1 : 0;
		netData.bPickContainers = gameData.bPickContainers ? 1 : 0;
		netData.bPickQuestItems = gameData.bPickQuestItems ? 1 : 0;
		netData.bNoStrongTargetFirst = gameData.bStrongTargetFirst ? 0 : 1;
	}

	void ConfigDataSerDe::Deserialize(const PRECEIVE_MUHELPER_DATA& netData, ConfigData& gameData)
	{
		gameData.iHuntingRange = static_cast<int>(netData.HuntingRange);

		gameData.iMaxSecondsAway = static_cast<int>(netData.DistanceMin);
		gameData.bLongRangeCounterAttack = (bool)netData.LongDistanceAttack;
		gameData.bReturnToOriginalPosition = (bool)netData.OriginalPosition;

		gameData.aiSkill.fill(0);
		gameData.aiSkill[0] = static_cast<int>(netData.BasicSkill1);
		gameData.aiSkill[1] = static_cast<int>(netData.ActivationSkill1);
		gameData.aiSkill[2] = static_cast<int>(netData.ActivationSkill2);

		gameData.aiSkillInterval.fill(0);
		gameData.aiSkillInterval[1] = static_cast<int>(netData.DelayMinSkill1);
		gameData.aiSkillInterval[2] = static_cast<int>(netData.DelayMinSkill2);

		gameData.aiSkillCondition.fill(0);
		gameData.aiSkillCondition[1] |= netData.Skill1Delay ? ON_TIMER : 0;
		gameData.aiSkillCondition[1] |= netData.Skill1Con ? ON_CONDITION : 0;
		gameData.aiSkillCondition[1] |= netData.Skill1PreCon == 0 ? ON_MOBS_NEARBY : ON_MOBS_ATTACKING;
		gameData.aiSkillCondition[1] |= netData.Skill1SubCon == 0 ? ON_MORE_THAN_TWO_MOBS :
			netData.Skill1SubCon == 1 ? ON_MORE_THAN_THREE_MOBS :
			netData.Skill1SubCon == 2 ? ON_MORE_THAN_FOUR_MOBS :
			netData.Skill1SubCon == 3 ? ON_MORE_THAN_FIVE_MOBS :
			0;

		gameData.aiSkillCondition[2] |= netData.Skill2Delay ? ON_TIMER : 0;
		gameData.aiSkillCondition[2] |= netData.Skill2Con ? ON_CONDITION : 0;
		gameData.aiSkillCondition[2] |= netData.Skill2PreCon == 0 ? ON_MOBS_NEARBY : ON_MOBS_ATTACKING;
		gameData.aiSkillCondition[2] |= netData.Skill2SubCon == 0 ? ON_MORE_THAN_TWO_MOBS :
			netData.Skill2SubCon == 1 ? ON_MORE_THAN_THREE_MOBS :
			netData.Skill2SubCon == 2 ? ON_MORE_THAN_FOUR_MOBS :
			netData.Skill2SubCon == 3 ? ON_MORE_THAN_FIVE_MOBS :
			0;
		gameData.bUseCombo = (bool)netData.Combo;

		gameData.aiBuff.fill(0);
		gameData.aiBuff[0] = static_cast<int>(netData.BuffSkill0NumberID);
		gameData.aiBuff[1] = static_cast<int>(netData.BuffSkill1NumberID);
		gameData.aiBuff[2] = static_cast<int>(netData.BuffSkill2NumberID);

		gameData.bBuffDuration = (bool)netData.BuffDuration;
		gameData.bBuffDurationParty = (bool)netData.BuffDurationforAllPartyMembers;
		gameData.iBuffCastInterval = static_cast<int>(netData.CastingBuffMin);

		gameData.bAutoHeal = (bool)netData.AutoHeal;
		gameData.iHealThreshold = static_cast<int>(netData.HPStatusAutoHeal) * 10;
		gameData.bUseDrainLife = static_cast<int>(netData.DrainLife);
		gameData.bUseHealPotion = (bool)netData.AutoPotion;
		gameData.iPotionThreshold = static_cast<int>(netData.HPStatusAutoPotion) * 10;
		gameData.iManaPotionThreshold = netData.MPStatusAutoPotion == 0
			? ConfigData::DefaultManaPotionThreshold
			: (static_cast<int>(netData.MPStatusAutoPotion) - 1) * 10;
		gameData.bSupportParty = (bool)netData.Party;
		gameData.bAutoHealParty = (bool)netData.PreferenceOfPartyHeal;
		gameData.iHealPartyThreshold = static_cast<int>(netData.HPStatusOfPartyMembers) * 10;

		gameData.bUseDarkRaven = (bool)netData.UseDarkSpirits;
		gameData.iDarkRavenMode = static_cast<int>(netData.PetAttack);
		gameData.bRepairItem = (bool)netData.RepairItem;

		gameData.iObtainingRange = static_cast<int>(netData.ObtainRange);
		gameData.bPickAllItems = (bool)netData.PickAllNearItems;
		gameData.bPickSelectItems = (bool)netData.PickSelectedItems;
		gameData.bPickZen = (bool)netData.Zen;
		gameData.bPickJewel = (bool)netData.JewelOrGem;
		gameData.bPickExcellent = (bool)netData.ExcellentItem;
		gameData.bPickAncient = (bool)netData.SetItem;
		gameData.bPickExtraItems = (bool)netData.AddExtraItem;

		gameData.aExtraItems.clear();
		// Older clients saved the names in the PC code page (wcstombs), CP1251 on a Russian Windows;
		// such a name is not valid UTF-8 and is read the old way. The next save writes it as UTF-8.
		const auto DecodeExtraItemName = [](const std::string& bytes) -> std::wstring
		{
			if (IsValidUtf8(bytes))
			{
				return StringUtils::NarrowToWide(bytes);
			}
			std::wstring legacy(bytes.size() + 1, L'\0');
			const size_t length = mbstowcs(legacy.data(), bytes.c_str(), legacy.size());
			if (length == static_cast<size_t>(-1))
			{
				return StringUtils::NarrowToWide(bytes);
			}
			legacy.resize(length);
			return legacy;
		};
		for (const auto& field : netData.ExtraItems)
		{
			// A full 15-byte name has no terminating zero.
			const std::string bytes(field, strnlen(field, MaxExtraItemBytes));
			if (!bytes.empty())
			{
				gameData.aExtraItems.insert(DecodeExtraItemName(bytes));
			}
		}

		// byte 33, client-local. server echoes unchanged, see MuHelperData.h
		gameData.bUseSelfDefense = (bool)netData.bUseSelfDefense;
		gameData.bAutoAcceptFriend = (bool)netData.bAutoAcceptFriend;
		gameData.bAutoAcceptGuild = (bool)netData.bAutoAcceptGuild;
		gameData.bFallbackBasicAttack = (bool)netData.bFallbackBasicAttack;
		gameData.bPickContainers = (bool)netData.bPickContainers;
		gameData.bPickQuestItems = (bool)netData.bPickQuestItems;
		gameData.bStrongTargetFirst = !netData.bNoStrongTargetFirst;
	}

}
