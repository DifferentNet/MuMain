/*+++++++++++++++++++++++++++++++++++++
    INCLUDE.
+++++++++++++++++++++++++++++++++++++*/
#include "stdafx.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/ZzzTexture.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Scenes/SceneCore.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "Character/CharacterManager.h"
#include "UI/Social/SocialWindowBase.h"
#include "UI/Core/WindowSystem.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Items/CSItemOption.h"
#include "I18N/All.h"
#include "Render/Text/CUIRenderText.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <iterator>
#include <string>
#include <vector>


namespace
{
constexpr std::uint8_t EMPTY_OPTION = 0xFF;

using FilePtr = std::unique_ptr<FILE, decltype(&std::fclose)>;

FilePtr OpenBinaryFile(const wchar_t* filename)
{
    return FilePtr(_wfopen(filename, L"rb"), &std::fclose);
}

void ReportFileIssue(const wchar_t* filename, const wchar_t* issue)
{
    wchar_t text[256]{};
    std::swprintf(text, std::size(text), L"%ls - %ls.", filename, issue);
    g_ErrorReport.Write(text);
    MessageBox(g_hWnd, text, nullptr, MB_OK);
    SendMessage(g_hWnd, WM_DESTROY, 0, 0);
}

// Webzen's set names are UTF-8, but two of them (Chrono's, Semeden's) carry a GB2312 apostrophe, the
// bytes A1 AF (A1 AE for the opening one), which UTF-8 shows as two unknown characters: "Chrono??s Ring of
// Magic". They become a plain apostrophe before the conversion.
void ReplaceGb2312Apostrophes(char* text, std::size_t size)
{
    std::size_t out = 0;
    for (std::size_t in = 0; in < size && text[in] != '\0'; ++in)
    {
        const auto lead = static_cast<unsigned char>(text[in]);
        const auto next = in + 1 < size ? static_cast<unsigned char>(text[in + 1]) : 0;
        if (lead == 0xA1 && (next == 0xAE || next == 0xAF))
        {
            text[out++] = '\'';
            ++in;
            continue;
        }

        text[out++] = text[in];
    }

    if (out < size)
    {
        text[out] = '\0';
    }
}

// Ancient items are named "<set> <kind>" (ghinzuro, 2026-10-23), so a set's name loses Webzen's possessive:
// "Warrior's" and "Mahes'" become "Warrior" and "Mahes", as "Enis" and "Apis" always were. The Portuguese and
// Spanish names ("do guerreiro, ") don't end in one and stay as they are.
void StripPossessive(char* text)
{
    const auto length = std::strlen(text);
    if (length > 2 && text[length - 2] == '\'' && text[length - 1] == 's')
    {
        text[length - 2] = '\0';
    }
    else if (length > 1 && text[length - 1] == '\'')
    {
        text[length - 1] = '\0';
    }
}

// The kind of an ancient item in its name, without the base item's own name (ghinzuro, 2026-10-23): "Enis Pants",
// not "Enis Legendary Pants". Armour keeps the last word of its name (Helm, Mask, Armor, Pants, Gloves, Boots),
// weapons and shields say what they are, rings and pendants keep their whole name, element included: Kantata's set
// has both a Ring of Poison and a Ring of Wind. English like the item names themselves, which have no Russian.
std::wstring AncientItemKind(const int itemType)
{
    const std::wstring name = ItemAttribute[itemType].Name;
    const auto contains = [&name](const wchar_t* word) { return name.find(word) != std::wstring::npos; };
    switch (itemType / MAX_ITEM_INDEX)
    {
    case ITEM_GROUP_SWORD:
        return L"Sword";
    case ITEM_GROUP_AXE:
        return L"Axe";
    case ITEM_GROUP_MACE:
        return contains(L"Scepter") ? L"Scepter" : L"Mace";
    case ITEM_GROUP_SPEAR:
        return L"Spear";
    case ITEM_GROUP_BOW:
        return contains(L"Crossbow") ? L"Crossbow" : L"Bow";
    case ITEM_GROUP_STAFF:
        return contains(L"Stick") ? L"Stick" : contains(L"Book") ? L"Book" : L"Staff";
    case ITEM_GROUP_SHIELD:
        return L"Shield";
    case ITEM_GROUP_HELM:
    case ITEM_GROUP_ARMOR:
    case ITEM_GROUP_PANTS:
    case ITEM_GROUP_GLOVES:
    case ITEM_GROUP_BOOTS:
    {
        const auto space = name.find_last_of(L' ');
        return space == std::wstring::npos ? name : name.substr(space + 1);
    }
    default:
        return name;
    }
}
}

static  CSItemOption csItemOption; // do not delete, required for singleton initialization.

namespace
{
// ItemSetOption_*.bmd: MAX_SET_OPTION records, each BuxConverted, then a checksum. A record holds the name, the
// tier option codes [6][2] and values, the ext option codes [2] and values, the option count, the full option
// codes [5] and values, and the class requirement [MAX_CLASS]. Version 1 (Webzen's) keeps the values in one byte,
// 110 bytes a record; version 2 (MU Moldova's scripts/ancient-options.py) in two, little-endian, 129 bytes.
constexpr std::size_t SET_OPTION_RECORD_V1 = 110;
constexpr std::size_t SET_OPTION_RECORD_V2 = 129;

class RecordReader
{
public:
    RecordReader(const std::uint8_t* data, const bool wideValues) : m_data(data), m_wideValues(wideValues) {}

    std::uint8_t Byte() { return m_data[m_offset++]; }

    std::uint16_t Value()
    {
        if (!m_wideValues)
        {
            return Byte();
        }

        const auto low = Byte();
        const auto high = Byte();
        return static_cast<std::uint16_t>(low | (high << 8));
    }

    void Text(char* target, const std::size_t size)
    {
        std::memcpy(target, m_data + m_offset, size);
        m_offset += size;
    }

private:
    const std::uint8_t* m_data;
    bool m_wideValues;
    std::size_t m_offset{};
};
} // namespace


bool CSItemOption::OpenItemSetScript()
{
    std::wstring strFileName = L"";
    const std::wstring strTest = L"";

    strFileName = L"Data\\Local\\ItemSetType" + strTest + L".bmd";
    if (!OpenItemSetType(strFileName.c_str()))		return false;

    strFileName = L"Data\\Local\\" + g_strSelectedML + L"\\ItemSetOption" + strTest + L"_" + g_strSelectedML + L".bmd";
    if (!OpenItemSetOption(strFileName.c_str()))	 	return false;
    return true;
}

bool CSItemOption::OpenItemSetType(const wchar_t* filename)
{
    auto file = OpenBinaryFile(filename);
    if (!file)
    {
        ReportFileIssue(filename, L"File does not exist.");
        return false;
    }

    const std::size_t entrySize = sizeof(ITEM_SET_TYPE);
    std::vector<std::uint8_t> buffer(entrySize * MAX_ITEM);
    if (std::fread(buffer.data(), buffer.size(), 1, file.get()) != 1)
    {
        ReportFileIssue(filename, L"Failed to read content.");
        return false;
    }

    std::uint32_t checksum{};
    if (std::fread(&checksum, sizeof(checksum), 1, file.get()) != 1)
    {
        ReportFileIssue(filename, L"Failed to read checksum.");
        return false;
    }

    if (checksum != GenerateCheckSum2(buffer.data(), static_cast<int>(buffer.size()), 0xE5F1))
    {
        ReportFileIssue(filename, L"File corrupted.");
        return false;
    }

    auto* pSeek = buffer.data();
    for (int i = 0; i < MAX_ITEM; ++i)
    {
        BuxConvert(pSeek, static_cast<int>(entrySize));
        std::memcpy(&m_ItemSetType[i], pSeek, entrySize);
        pSeek += entrySize;
    }

    return true;
}

bool CSItemOption::OpenItemSetOption(const wchar_t* filename)
{
    auto file = OpenBinaryFile(filename);
    if (!file)
    {
        ReportFileIssue(filename, L"File does not exist.");
        return false;
    }

    std::fseek(file.get(), 0, SEEK_END);
    const auto fileSize = static_cast<std::size_t>(std::ftell(file.get()));
    std::fseek(file.get(), 0, SEEK_SET);
    const bool wideValues = fileSize == SET_OPTION_RECORD_V2 * MAX_SET_OPTION + sizeof(std::uint32_t);
    if (!wideValues && fileSize != SET_OPTION_RECORD_V1 * MAX_SET_OPTION + sizeof(std::uint32_t))
    {
        ReportFileIssue(filename, L"Unknown file size.");
        return false;
    }

    const std::size_t entrySize = wideValues ? SET_OPTION_RECORD_V2 : SET_OPTION_RECORD_V1;
    std::vector<std::uint8_t> buffer(entrySize * MAX_SET_OPTION);
    if (std::fread(buffer.data(), buffer.size(), 1, file.get()) != 1)
    {
        ReportFileIssue(filename, L"Failed to read content.");
        return false;
    }

    std::uint32_t checksum{};
    if (std::fread(&checksum, sizeof(checksum), 1, file.get()) != 1)
    {
        ReportFileIssue(filename, L"Failed to read checksum.");
        return false;
    }

    if (checksum != GenerateCheckSum2(buffer.data(), static_cast<int>(buffer.size()), 0xA2F1))
    {
        ReportFileIssue(filename, L"File corrupted.");
        return false;
    }

    auto* pSeek = buffer.data();
    for (int i = 0; i < MAX_SET_OPTION; ++i)
    {
        BuxConvert(pSeek, static_cast<int>(entrySize));

        RecordReader record(pSeek, wideValues);
        auto* target = &m_ItemSetOption[i];

        char setName[MAX_ITEM_SET_NAME + 1]{};
        record.Text(setName, MAX_ITEM_SET_NAME);
        ReplaceGb2312Apostrophes(setName, MAX_ITEM_SET_NAME);
        StripPossessive(setName);
        CMultiLanguage::ConvertFromUtf8(target->strSetName, setName);
        for (auto& tier : target->byStandardOption)
            for (auto& code : tier)
                code = record.Byte();
        for (auto& tier : target->byStandardOptionValue)
            for (auto& value : tier)
                value = record.Value();
        for (auto& code : target->byExtOption)
            code = record.Byte();
        for (auto& value : target->byExtOptionValue)
            value = record.Value();
        target->byOptionCount = record.Byte();
        target->bySetItemCount = 0; // Is calculated below
        for (auto& code : target->byFullOption)
            code = record.Byte();
        for (auto& value : target->byFullOptionValue)
            value = record.Value();
        for (auto& requirement : target->byRequireClass)
            requirement = record.Byte();

        pSeek += entrySize;
    }

    for (int j = 0; j < MAX_ITEM; ++j)
    {
        const auto& temptype = m_ItemSetType[j];
        for (int k = 0; k < MAX_ITEM_SETS_PER_ITEM; ++k)
        {
            const auto optionNumber = temptype.byOption[k];
            if (optionNumber < MAX_SET_OPTION)
            {
                m_ItemSetOption[optionNumber].bySetItemCount++;
            }
        }
    }

    return true;
}

std::uint8_t CSItemOption::IsChangeSetItem(const int Type, const int SubType)
{
    const ITEM_SET_TYPE& itemSType = m_ItemSetType[Type];

    if (SubType == -1)
    {
        return itemSType.byOption[0] == EMPTY_OPTION && itemSType.byOption[1] == EMPTY_OPTION ? 0 : 255;
    }

    return itemSType.byOption[SubType] == EMPTY_OPTION ? 0 : static_cast<std::uint8_t>(SubType + 1);
}

std::uint16_t CSItemOption::GetMixItemLevel(const int Type) const
{
    if (Type < 0) return 0;

    const ITEM_SET_TYPE& itemSType = m_ItemSetType[Type];

    return static_cast<std::uint16_t>(itemSType.byMixItemLevel[0] | (static_cast<std::uint16_t>(itemSType.byMixItemLevel[1]) << 8));
}

bool CSItemOption::GetAncientItemName(wchar_t* target, const std::size_t targetSize, const int itemType,
                                      const int ancientDiscriminator) const
{
    const int setItemType = ancientDiscriminator % 0x04;
    if (setItemType <= 0 || itemType < 0 || itemType >= MAX_ITEM || target == nullptr || targetSize == 0)
    {
        return false;
    }

    const auto setNumber = m_ItemSetType[itemType].byOption[setItemType - 1];
    if (setNumber == EMPTY_OPTION || setNumber == 0)
    {
        return false;
    }

    const std::wstring name = std::wstring(m_ItemSetOption[setNumber].strSetName) + L" " + AncientItemKind(itemType);
    const auto length = std::min(name.size(), targetSize - 1);
    std::wmemcpy(target, name.c_str(), length);
    target[length] = L'\0';
    return true;
}


void CSItemOption::checkItemType(SET_SEARCH_RESULT* optionList, const int iType, const int ancientDiscriminator) const
{
    if (ancientDiscriminator <= 0)
    {
        return;
    }

    const auto setTypeIndex = static_cast<std::uint8_t>(ancientDiscriminator - 1);

    const ITEM_SET_TYPE& itemSetType = m_ItemSetType[iType];
    const auto itemSetNumber = itemSetType.byOption[setTypeIndex];
    
    if (itemSetNumber != 255 && itemSetNumber != 0)
    {
        // add set item to list
        for (int i = 0; i < MAX_EQUIPPED_SET_ITEMS; ++i)
        {
            const auto current = &optionList[i];
            if (current->SetNumber == 0)
            {
                // The set wasn't found in another item yet, so add it
                current->SetNumber = itemSetNumber;
                current->CompleteSetItemCount = m_ItemSetOption[itemSetNumber].bySetItemCount;
                current->ItemCount++;
                current->SetTypeIndex = setTypeIndex;
                wcscpy(current->SetName, m_ItemSetOption[itemSetNumber].strSetName);
                break;
            }

            if (current->SetNumber == itemSetNumber)
            {
                current->ItemCount++;
                current->SetTypeIndex = setTypeIndex;
                break;
            }
        }
    }
}


bool CSItemOption::isClassRequirementFulfilled(const ITEM_SET_OPTION& setOptions, const int firstClass, int secondClass)
{
    bool RequireClass = false;
    for (int i = CLASS_WIZARD; i <= CLASS_RAGEFIGHTER; i++)
    {
        if (setOptions.byRequireClass[i] == 1 && firstClass == i)
        {
            RequireClass = true;
        }

        if (setOptions.byRequireClass[i] == 2 && firstClass == i && secondClass)
        {
            RequireClass = true;
        }
    }

    return RequireClass;
}

void CSItemOption::TryAddSetOption(std::uint8_t option, int value, int optionIndex, SET_SEARCH_RESULT_OPT& set, const ITEM_SET_OPTION& setOptions, bool isThisSetComplete, bool isFullOption, bool isExtOption, bool fulfillsClassRequirement, int firstClass, int secondClass)
{
    if (option == EMPTY_OPTION
        || value == 0
        || (option >= MASTERY_OPTION && option <= LAST_MASTERY_OPTION
            && (setOptions.byRequireClass[firstClass] && secondClass >= setOptions.byRequireClass[firstClass] - 1)))
    {
        return;
    }

    const auto setOption = &set.SetOption[set.SetOptionCount];
    setOption->OptionNumber = option;
    setOption->FulfillsClassRequirement = fulfillsClassRequirement;
    setOption->Value = value;
    setOption->IsActive = isThisSetComplete || optionIndex < set.ItemCount - 1;
    setOption->IsFullOption = isFullOption;
    setOption->IsExtOption = isExtOption;

    set.SetOptionCount++;
}

void CSItemOption::calcSetOptionList(const SET_SEARCH_RESULT* optionList)
{
    const int firstClass = gCharacterManager.GetBaseClass(Hero->Class);
    const int secondClass = gCharacterManager.IsSecondClass(Hero->Class);

    int setCount = 0;
    for (int i = 0; i < MAX_EQUIPPED_SET_ITEMS; ++i)
    {
        if (optionList[i].SetNumber == 0)
        {
            break;
        }

        if (optionList[i].ItemCount >= 2)
        {
            memcpy(&m_SetSearchResult[setCount], &optionList[i], sizeof(SET_SEARCH_RESULT));
            setCount++;
        }
    }

    m_SetSearchResultCount = setCount;

    // now we have all our equipped sets together and can continue to
    // build up the corresponding option list

    m_bySameSetItem = 0;

    Hero->ExtendState = 0;
    for (int i = 0; i < m_SetSearchResultCount; ++i)
    {
        if (AddSetOptions(m_SetSearchResult[i], firstClass, secondClass))
        {
            Hero->ExtendState = 1;
        }
    }
}

bool CSItemOption::AddSetOptions(SET_SEARCH_RESULT_OPT& set, const int firstClass, const int secondClass) const
{
    const ITEM_SET_OPTION& setOptions = m_ItemSetOption[set.SetNumber];
    const bool isThisSetComplete = set.CompleteSetItemCount <= set.ItemCount;

    const auto requireClass = isClassRequirementFulfilled(setOptions, firstClass, secondClass);
    const auto standardOptionCount = std::min<int>(set.CompleteSetItemCount - 1, MAX_ITEM_SET_STANDARD_OPTION_COUNT);
    for (int o = 0; o < standardOptionCount; ++o)
    {
        for (int n = 0; n < MAX_ITEM_SET_STANDARD_OPTION_PER_ITEM_COUNT; ++n)
        {
            TryAddSetOption(setOptions.byStandardOption[o][n], setOptions.byStandardOptionValue[o][n], o, set, setOptions, isThisSetComplete, false, false, requireClass, firstClass, secondClass);
        }
    }

    for (int o = 0; o < MAX_ITEM_SET_EXT_OPTION_COUNT; ++o)
    {
        TryAddSetOption(setOptions.byExtOption[o], setOptions.byExtOptionValue[o], 0, set, setOptions, isThisSetComplete, false, true, requireClass, firstClass, secondClass);
    }

    for (int o = 0; o < MAX_ITEM_SET_FULL_OPTION_COUNT; ++o)
    {
        TryAddSetOption(setOptions.byFullOption[o], setOptions.byFullOptionValue[o], 255, set, setOptions, isThisSetComplete, true, false, requireClass, firstClass, secondClass);
    }

    return isThisSetComplete;
}

bool CSItemOption::getExplainText(wchar_t* text, std::uint8_t option, int value)
{
    if (option == EMPTY_OPTION)
    {
        return false;
    }

    if (option == SET_OPTION_PHYSICAL_DAMAGE_PERCENT)
    {
        mu_swprintf(text, I18N::Game::IncreasePhysicalDamagePercentD, value);
        return true;
    }

    switch (option + AT_SET_OPTION_IMPROVE_STRENGTH)
    {
    case AT_SET_OPTION_IMPROVE_MAGIC_POWER:
        mu_swprintf(text, I18N::Game::IncreaseWizardryDmgD, value);
        return true;

    case AT_SET_OPTION_IMPROVE_STRENGTH:
    case AT_SET_OPTION_IMPROVE_DEXTERITY:
    case AT_SET_OPTION_IMPROVE_ENERGY:
    case AT_SET_OPTION_IMPROVE_VITALITY:
    case AT_SET_OPTION_IMPROVE_CHARISMA:
    case AT_SET_OPTION_IMPROVE_ATTACK_MIN:
    case AT_SET_OPTION_IMPROVE_ATTACK_MAX:
        mu_swprintf(text, I18N::Game::Lookup(950 + option), value);
        return true;

    case AT_SET_OPTION_IMPROVE_DAMAGE:
    case AT_SET_OPTION_IMPROVE_ATTACKING_PERCENT:
    case AT_SET_OPTION_IMPROVE_DEFENCE:
    case AT_SET_OPTION_IMPROVE_MAX_LIFE:
    case AT_SET_OPTION_IMPROVE_MAX_MANA:
    case AT_SET_OPTION_IMPROVE_MAX_AG:
    case AT_SET_OPTION_IMPROVE_ADD_AG:
    case AT_SET_OPTION_IMPROVE_CRITICAL_DAMAGE_PERCENT:
    case AT_SET_OPTION_IMPROVE_CRITICAL_DAMAGE:
    case AT_SET_OPTION_IMPROVE_EXCELLENT_DAMAGE_PERCENT:
    case AT_SET_OPTION_IMPROVE_EXCELLENT_DAMAGE:
    case AT_SET_OPTION_IMPROVE_SKILL_ATTACK:
    case AT_SET_OPTION_DOUBLE_DAMAGE:
        mu_swprintf(text, I18N::Game::Lookup(949 + option), value);
        return true;

    case AT_SET_OPTION_DISABLE_DEFENCE:
        mu_swprintf(text, I18N::Game::IgnoreEnemiesDefensiveSkillD, value);
        return true;

    case AT_SET_OPTION_TWO_HAND_SWORD_IMPROVE_DAMAGE:
        mu_swprintf(text, I18N::Game::IncreaseDamageWhenUsingTwoHandedWeaponsD, value);
        return true;

    case AT_SET_OPTION_IMPROVE_SHIELD_DEFENCE:
        mu_swprintf(text, I18N::Game::IncreaseDefensiveSkillWhenUsingShieldWeaponsD, value);
        return true;

    case AT_SET_OPTION_IMPROVE_ATTACK_1:
    case AT_SET_OPTION_IMPROVE_ATTACK_2:
    case AT_SET_OPTION_IMPROVE_MAGIC:
        //	case AT_SET_OPTION_IMPROVE_DEFENCE_1:
        //	case AT_SET_OPTION_IMPROVE_DEFENCE_2:
    case AT_SET_OPTION_IMPROVE_DEFENCE_3:
    case AT_SET_OPTION_IMPROVE_DEFENCE_4:
    case AT_SET_OPTION_FIRE_MASTERY:
    case AT_SET_OPTION_ICE_MASTERY:
    case AT_SET_OPTION_THUNDER_MASTERY:
    case AT_SET_OPTION_POSION_MASTERY:
    case AT_SET_OPTION_WATER_MASTERY:
    case AT_SET_OPTION_WIND_MASTERY:
    case AT_SET_OPTION_EARTH_MASTERY:
        mu_swprintf(text, I18N::Game::Lookup(971 + (option + AT_SET_OPTION_IMPROVE_STRENGTH - AT_SET_OPTION_IMPROVE_ATTACK_2)), value);
        return true;
    default:
        return false;
    }
}

int CSItemOption::AggregateOptionValue(int optionNumber) const
{
    int result = 0;
    for (int i = 0; i < m_SetSearchResultCount; i++)
    {
        const auto& set = m_SetSearchResult[i];
        for (int j = 0; j < set.SetOptionCount; j++)
        {
            const auto& option = set.SetOption[j];

            if (option.IsActive
                && option.OptionNumber == optionNumber
                && option.Value != 0)
            {
                result += option.Value;
            }
        }
    }

    return result;
}

void CSItemOption::PlusSpecial(std::uint16_t* Value, int Special) const
{
    Special -= AT_SET_OPTION_IMPROVE_STRENGTH;

    if (const int optionValue = AggregateOptionValue(Special))
    {
        *Value += optionValue;
    }
}

void CSItemOption::PlusSpecialPercent(std::uint16_t* Value, int Special) const
{
    Special -= AT_SET_OPTION_IMPROVE_STRENGTH;

    if (const int optionValue = AggregateOptionValue(Special))
    {
        *Value += ((*Value) * optionValue) / 100;
    }
}

void CSItemOption::PlusSpecialLevel(std::uint16_t* Value, std::uint16_t SrcValue, int Special) const
{
    Special -= AT_SET_OPTION_IMPROVE_STRENGTH;
    int optionValue = 0;
    int count = 0;

    for (int i = 0; i < m_SetSearchResultCount; i++)
    {
        const auto& set = m_SetSearchResult[i];
        for (int j = 0; j < set.SetOptionCount; j++)
        {
            const auto& option = set.SetOption[j];
            if (option.IsActive
                && option.OptionNumber == Special
                && option.Value != 0)
            {
                optionValue += option.Value;
                count++;
            }
        }
    }

    if (optionValue)
    {
        optionValue = SrcValue * optionValue / 100;
        *Value += (optionValue * count);
    }
}

void CSItemOption::PlusMastery(int* Value, std::uint8_t MasteryType) const
{
    int optionValue = 0;

    for (int i = 0; i < m_SetSearchResultCount; i++)
    {
        const auto& set = m_SetSearchResult[i];
        for (int j = 0; j < set.SetOptionCount; j++)
        {
            const auto& option = set.SetOption[j];
            if (option.IsActive
                && option.OptionNumber >= MASTERY_OPTION
                && (option.OptionNumber - MASTERY_OPTION - 5) == MasteryType
                && option.Value != 0)
            {
                optionValue += option.Value;
            }
        }
    }

    if (optionValue)
    {
        *Value += optionValue;
    }
}

int CSItemOption::GetDefaultOptionValue(ITEM* ip, std::uint16_t* Value)
{
    if (ip->Type > MAX_ITEM)
    {
        return -1;
    }

    *Value = ip->AncientBonusOption;

    const ITEM_ATTRIBUTE* p = &ItemAttribute[ip->Type];

    return p->AttType;
}

bool CSItemOption::GetDefaultOptionText(const ITEM* ip, wchar_t* Text)
{
    if (ip->Type > MAX_ITEM)
    {
        return false;
    }

    if (ip->AncientBonusOption <= 0)
    {
        return false;
    }

    switch (ItemAttribute[ip->Type].AttType)
    {
    case SET_OPTION_STRENGTH:
        mu_swprintf(Text, I18N::Game::IncreaseStrengthD, ip->AncientBonusOption * 5);
        break;

    case SET_OPTION_DEXTERITY:
        mu_swprintf(Text, I18N::Game::IncreaseAgilityD, ip->AncientBonusOption * 5);
        break;

    case SET_OPTION_ENERGY:
        mu_swprintf(Text, I18N::Game::IncreaseEnergyD, ip->AncientBonusOption * 5);
        break;

    case SET_OPTION_VITALITY:
        mu_swprintf(Text, I18N::Game::IncreaseStaminaD, ip->AncientBonusOption * 5);
        break;

    default:
        return false;
    }
    return true;
}

bool CSItemOption::IsNonWeaponSkillOrIsSkillEquipped(ActionSkillType skill)
{
    bool checkForWeaponSkill;
    auto baseSkill = gSkillManager.MasterSkillToBaseSkillIndex(skill);
    switch (baseSkill)
    {
    case AT_SKILL_POWER_SLASH:
    case AT_SKILL_TRIPLE_SHOT:
    case AT_SKILL_FALLING_SLASH:
    case AT_SKILL_LUNGE:
    case AT_SKILL_UPPERCUT:
    case AT_SKILL_CYCLONE:
    case AT_SKILL_SLASH:
        checkForWeaponSkill = true;
        break;
    default:
        checkForWeaponSkill = false;
        break;
    }

    if (!checkForWeaponSkill)
    {
        return true;
    }

    for (int i = 0; i < 2; i++)
    {
        const ITEM* item = &CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT + i];
        if (item == nullptr || item->Type <= -1)
        {
            continue;
        }

        for (int j = 0; j < item->SpecialNum; j++)
        {
            if (item->Special[j] == baseSkill)
            {
                return true;
            }
        }
    }

    return false;
}

int CSItemOption::RenderDefaultOptionText(const ITEM* ip, int TextNum)
{
    int TNum = TextNum;
    if (GetDefaultOptionText(ip, TextList[TNum]))
    {
        TextListColor[TNum] = TEXT_COLOR_BLUE;
        TNum++;

        if ((ip->Type >= ITEM_RING_OF_ICE && ip->Type <= ITEM_RING_OF_POISON) || (ip->Type >= ITEM_PENDANT_OF_LIGHTING && ip->Type <= ITEM_PENDANT_OF_FIRE) || (ip->Type >= ITEM_RING_OF_FIRE && ip->Type <= ITEM_PENDANT_OF_WATER))
        {
            mu_swprintf(TextList[TNum], I18N::Game::IncreaseAttributeDamage); // "Increase Attribute Damage"
            TextListColor[TNum] = TEXT_COLOR_BLUE;
            TNum++;
        }
    }

    return TNum;
}

void CSItemOption::getAllAddState(std::uint16_t* Strength, std::uint16_t* Dexterity, std::uint16_t* Energy, std::uint16_t* Vitality, std::uint16_t* Charisma) const
{
    for (int i = EQUIPMENT_WEAPON_RIGHT; i < MAX_EQUIPMENT; ++i)
    {
        ITEM* item = &CharacterMachine->Equipment[i];

        if (item->Durability <= 0)
        {
            continue;
        }

        std::uint16_t Result = 0;
        switch (GetDefaultOptionValue(item, &Result))
        {
        case SET_OPTION_STRENGTH:
            *Strength += Result * 5;
            break;

        case SET_OPTION_DEXTERITY:
            *Dexterity += Result * 5;
            break;

        case SET_OPTION_ENERGY:
            *Energy += Result * 5;
            break;

        case SET_OPTION_VITALITY:
            *Vitality += Result * 5;
            break;
        }
    }

    AddStatsBySetOptions(Strength, Dexterity, Energy, Vitality, Charisma);
}

void    CSItemOption::AddStatsBySetOptions(std::uint16_t* Strength, std::uint16_t* Dexterity, std::uint16_t* Energy, std::uint16_t* Vitality, std::uint16_t* Charisma) const
{
    for (int i = 0; i < m_SetSearchResultCount; i++)
    {
        auto& set = m_SetSearchResult[i];
        for (int j = 0; j < set.SetOptionCount; j++)
        {
            auto& option = set.SetOption[j];
            if (!option.IsActive)
            {
                continue;
            }

            switch (option.OptionNumber)
            {
            case AT_SET_OPTION_IMPROVE_STRENGTH:
                *Strength += option.Value;
                break;

            case AT_SET_OPTION_IMPROVE_DEXTERITY:
                *Dexterity += option.Value;
                break;

            case AT_SET_OPTION_IMPROVE_ENERGY:
                *Energy += option.Value;
                break;

            case AT_SET_OPTION_IMPROVE_VITALITY:
                *Vitality += option.Value;
                break;

            case AT_SET_OPTION_IMPROVE_CHARISMA:
                *Charisma += option.Value;
                break;
            default:
                // other options are not handled here.
                break;
            }
        }
    }
}

void CSItemOption::getAllAddStateOnlyAddValue(std::uint16_t* AddStrength, std::uint16_t* AddDexterity, std::uint16_t* AddEnergy, std::uint16_t* AddVitality, std::uint16_t* AddCharisma) const
{
    *AddStrength = *AddDexterity = *AddEnergy = *AddVitality = *AddCharisma = 0;
    getAllAddState(AddStrength, AddDexterity, AddEnergy, AddVitality, AddCharisma);
}

void CSItemOption::getAllAddOptionStatesbyCompare(std::uint16_t* Strength, std::uint16_t* Dexterity, std::uint16_t* Energy, std::uint16_t* Vitality, std::uint16_t* Charisma, std::uint16_t iCompareStrength, std::uint16_t iCompareDexterity, std::uint16_t iCompareEnergy, std::uint16_t iCompareVitality, std::uint16_t iCompareCharisma)
{
    for (int i = EQUIPMENT_WEAPON_RIGHT; i < MAX_EQUIPMENT; ++i)
    {
        ITEM* item = &CharacterMachine->Equipment[i];

        if (item->RequireStrength > iCompareStrength ||
            item->RequireDexterity > iCompareDexterity ||
            item->RequireEnergy > iCompareEnergy)
        {
            continue;
        }

        if (item->Durability <= 0)
        {
            continue;
        }

        std::uint16_t Result = 0;
        switch (GetDefaultOptionValue(item, &Result))
        {
        case SET_OPTION_STRENGTH:
            *Strength += Result * 5;
            break;

        case SET_OPTION_DEXTERITY:
            *Dexterity += Result * 5;
            break;

        case SET_OPTION_ENERGY:
            *Energy += Result * 5;
            break;

        case SET_OPTION_VITALITY:
            *Vitality += Result * 5;
            break;
        }
    }

    AddStatsBySetOptions(Strength, Dexterity, Energy, Vitality, Charisma);
}

void CSItemOption::CheckItemSetOptions()
{
    SET_SEARCH_RESULT byOptionList[MAX_EQUIPPED_SET_ITEMS] = { };

    const ITEM* itemRight = nullptr;

    std::fill(std::begin(m_SetSearchResult), std::end(m_SetSearchResult), SET_SEARCH_RESULT_OPT{});

    for (int i = 0; i < MAX_EQUIPMENT_INDEX; ++i)
    {
        if (i == EQUIPMENT_WING || i == EQUIPMENT_HELPER)
        {
            continue;
        }

        const ITEM* item = &CharacterMachine->Equipment[i];

        if (item->Durability <= 0)
        {
            continue;
        }

        if ((i == EQUIPMENT_WEAPON_LEFT || i == EQUIPMENT_RING_LEFT)
            && itemRight != nullptr && itemRight->Type == item->Type
            && (itemRight->AncientDiscriminator == item->AncientDiscriminator))
        {
            // same item of a set should only count once
            continue;
        }

        if (item->Type > -1)
        {
            checkItemType(byOptionList, item->Type, item->AncientDiscriminator);
        }

        if (i == EQUIPMENT_WEAPON_RIGHT || i == EQUIPMENT_RING_RIGHT)
        {
            itemRight = item;
        }
    }

    calcSetOptionList(byOptionList);
    getAllAddStateOnlyAddValue(&CharacterAttribute->AddStrength, &CharacterAttribute->AddDexterity, &CharacterAttribute->AddEnergy, &CharacterAttribute->AddVitality, &CharacterAttribute->AddCharisma);

    const auto AllStrength = static_cast<std::uint16_t>(CharacterAttribute->Strength + CharacterAttribute->AddStrength);
    const auto AllDexterity = static_cast<std::uint16_t>(CharacterAttribute->Dexterity + CharacterAttribute->AddDexterity);
    const auto AllEnergy = static_cast<std::uint16_t>(CharacterAttribute->Energy + CharacterAttribute->AddEnergy);
    auto AllVitality = static_cast<std::uint16_t>(CharacterAttribute->Vitality + CharacterAttribute->AddVitality);
    const auto AllCharisma = static_cast<std::uint16_t>(CharacterAttribute->Charisma + CharacterAttribute->AddCharisma);
    const auto AllLevel = static_cast<std::uint16_t>(CharacterAttribute->Level);

    // And now we're doing all that again, just for checking the required stats?!
    // TODO: How can this be improved?

    std::fill(std::begin(m_SetSearchResult), std::end(m_SetSearchResult), SET_SEARCH_RESULT_OPT{});
    std::fill(std::begin(byOptionList), std::end(byOptionList), SET_SEARCH_RESULT{});

    for (int i = 0; i < MAX_EQUIPMENT_INDEX; ++i)
    {
        if (i == EQUIPMENT_WING || i == EQUIPMENT_HELPER)
        {
            continue;
        }

        ITEM* ip = &CharacterMachine->Equipment[i];

        if (ip->RequireDexterity > AllDexterity || ip->RequireEnergy > AllEnergy || ip->RequireStrength > AllStrength || ip->RequireLevel > AllLevel || ip->RequireCharisma > AllCharisma || ip->Durability <= 0 || (IsRequireEquipItem(ip) == false)) {
            continue;
        }

        if ((i == EQUIPMENT_WEAPON_LEFT || i == EQUIPMENT_RING_LEFT)
            && itemRight != nullptr && itemRight->Type == ip->Type &&
            itemRight->AncientDiscriminator == ip->AncientDiscriminator)
        {
            continue;
        }

        if (ip->Type > -1)
        {
            checkItemType(byOptionList, ip->Type, ip->AncientDiscriminator);
        }

        if (i == EQUIPMENT_WEAPON_RIGHT || i == EQUIPMENT_RING_RIGHT)
        {
            itemRight = ip;
        }
    }

    calcSetOptionList(byOptionList);
}

void CSItemOption::MoveSetOptionList(const int StartX, const int StartY)
{
    int x, y, Width, Height;

    Width = 162; Height = 20; x = StartX + 14; y = StartY + 22;
    if (MouseX >= x && MouseX < x + Width && MouseY >= y && MouseY < y + Height)
    {
        m_bViewOptionList = true;

        MouseLButtonPush = false;
        MouseUpdateTime = 0;
        MouseUpdateTimeMax = 6;
    }
}

void CSItemOption::RenderSetOptionButton(const int StartX, const int StartY)
{
    float x, y, Width, Height;
    wchar_t  Text[100];

    Width = 162.f; Height = 20.f; x = (float)StartX + 14; y = (float)StartY + 22;
    RenderBitmap(BITMAP_INTERFACE_EX + 21, x, y, Width, Height, 0.f, 0.f, Width / 256.f, Height / 32.f);

    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetTextColor(0, 0, 0, 255);
    g_pRenderText->SetBgColor(100, 0, 0, 0);
    mu_swprintf(Text, L"[%ls]", I18N::Game::SetOption);
    g_pRenderText->RenderText(StartX + 96, (int)(y + 3), Text, 0, 0, RT3_WRITE_CENTER);

    g_pRenderText->SetTextColor(0xffffffff);
    if (m_SetSearchResultCount > 0)
        g_pRenderText->SetTextColor(255, 204, 25, 255);
    else
        g_pRenderText->SetTextColor(128, 128, 128, 255);
    g_pRenderText->RenderText(StartX + 95, (int)(y + 2), Text, 0, 0, RT3_WRITE_CENTER);
    g_pRenderText->SetTextColor(255, 255, 255, 255);
}

bool CSItemOption::BuildSetOptionTooltipModel(UI::Inventory::Tooltip::Model& outModel)
{
    // Hover gating is the caller's responsibility now (CMyInventory::SyncRmlModel(), driven by
    // RmlUi's own mouseover/mouseout on the Set Option label) -- this only needs to know whether
    // there's anything to show.
    if (m_SetSearchResultCount == 0)
        return false;

    std::uint8_t TextNum = 0;

    mu_swprintf(TextList[TextNum], L"\n"); TextListColor[TextNum] = 0; TextBold[TextNum] = false; TextNum++;
    mu_swprintf(TextList[TextNum], L"\n"); TextListColor[TextNum] = 0; TextBold[TextNum] = false; TextNum++;
    mu_swprintf(TextList[TextNum], L"\n"); TextListColor[TextNum] = 0; TextBold[TextNum] = false; TextNum++;

    for (int i = 0; i < m_SetSearchResultCount; i++)
    {
        const auto& set = m_SetSearchResult[i];

        // print set name:
        mu_swprintf(TextList[TextNum], L"%ls %ls", set.SetName, I18N::Game::Set);
        TextListColor[TextNum] = TEXT_COLOR_YELLOW;
        TextBold[TextNum] = true;
        TextNum++;

        TextNum = RenderSetOptionList(set, TextNum, true, false);
    }

    outModel.Reset();
    outModel.count = std::min<int>(TextNum, UI::Inventory::Tooltip::MAX_TOOLTIP_LINES);
    for (int i = 0; i < outModel.count; ++i)
    {
        UI::Inventory::Tooltip::Line& line = outModel.lines[i];
        wcsncpy(line.text, TextList[i], UI::Inventory::Tooltip::MAX_TOOLTIP_LINE_TEXT - 1);
        line.text[UI::Inventory::Tooltip::MAX_TOOLTIP_LINE_TEXT - 1] = L'\0';
        switch (TextListColor[i])
        {
        case TEXT_COLOR_BLUE:   line.color = UI::Inventory::Tooltip::LineColor::Blue;   break;
        case TEXT_COLOR_YELLOW: line.color = UI::Inventory::Tooltip::LineColor::Yellow; break;
        case TEXT_COLOR_GREEN:  line.color = UI::Inventory::Tooltip::LineColor::Green;  break;
        case TEXT_COLOR_PURPLE: line.color = UI::Inventory::Tooltip::LineColor::Purple; break;
        default:                line.color = UI::Inventory::Tooltip::LineColor::White;  break;
        }
        line.isBold = TextBold[i] != 0;
    }

    return true;
}

void CSItemOption::CheckRenderOptionHelper(const wchar_t* FilterName)
{
    wchar_t Name[256];

    if (FilterName[0] != '/') return;

    const auto Length1 = wcslen(FilterName);
    for (int i = 0; i < MAX_SET_OPTION; ++i)
    {
        ITEM_SET_OPTION& setOption = m_ItemSetOption[i];
        if (setOption.byOptionCount < 255)
        {
            mu_swprintf(Name, L"/%ls", setOption.strSetName);

            const auto Length2 = wcslen(Name);

            m_byRenderOptionList = 0;
            if (wcsncmp(FilterName, Name, Length1) == 0 && wcsncmp(FilterName, Name, Length2) == 0)
            {
                g_pNewUISystem->Hide(mu::ui::window::INTERFACE_ITEM_EXPLANATION);
                g_pNewUISystem->Hide(mu::ui::window::INTERFACE_HELP);
                g_pNewUISystem->Show(mu::ui::window::INTERFACE_SETITEM_EXPLANATION);

                m_byRenderOptionList = static_cast<std::uint8_t>(i + 1);
                return;
            }
        }
    }
}

int CSItemOption::RenderSetOptionListInItem(const ITEM* ip, int TextNum, bool bIsEquippedItem)
{
    if (ip->AncientDiscriminator == 0)
    {
        return TextNum;
    }

    const ITEM_SET_TYPE& itemSType = m_ItemSetType[ip->Type];

    m_bySelectedItemOption = itemSType.byOption[ip->AncientDiscriminator - 1];

    if (m_bySelectedItemOption <= 0 || m_bySelectedItemOption == 255) return TextNum;

    int TNum = TextNum;

    const ITEM_SET_OPTION& setOption = m_ItemSetOption[m_bySelectedItemOption];
    if (setOption.byOptionCount >= 255)
    {
        m_bySelectedItemOption = 0;
        return TNum;
    }


    mu_swprintf(TextList[TNum], L"\n"); TNum++;
    mu_swprintf(TextList[TNum], L"%ls %ls", I18N::Game::Set, I18N::Game::ItemOptionInfo);
    TextListColor[TNum] = TEXT_COLOR_YELLOW;
    TNum++;

    mu_swprintf(TextList[TNum], L"\n"); TNum++;
    mu_swprintf(TextList[TNum], L"\n"); TNum++;


    bool setIsWorn = false;
    for (int i = 0; i < m_SetSearchResultCount; i++)
    {
        const auto& set = m_SetSearchResult[i];
        if (wcscmp(set.SetName, setOption.strSetName) == 0)
        {
            // Set Found.
            TNum = RenderSetOptionList(set, TNum, bIsEquippedItem, true);
            setIsWorn = true;
            break;
        }
    }

    if (!setIsWorn)
    {
        // m_SetSearchResult holds only the sets with two or more items worn, so an item of any other set
        // (in the inventory, a shop, a trade, or worn alone) listed no options under its header. Its options
        // are listed all the same, none of them active, as in the original client.
        SET_SEARCH_RESULT_OPT set{};
        set.SetNumber = m_bySelectedItemOption;
        set.CompleteSetItemCount = setOption.bySetItemCount;
        set.SetTypeIndex = static_cast<std::uint8_t>(ip->AncientDiscriminator - 1);
        AddSetOptions(set, gCharacterManager.GetBaseClass(Hero->Class), gCharacterManager.IsSecondClass(Hero->Class));
        TNum = RenderSetOptionList(set, TNum, bIsEquippedItem, true);
    }

    TNum = RenderPowerOfTheAncients(TNum);
    TNum = RenderSetPieceList(m_bySelectedItemOption, TNum);

    mu_swprintf(TextList[TNum], L"\n"); TNum++;
    mu_swprintf(TextList[TNum], L"\n"); TNum++;

    return TNum;
}

namespace
{
// Whether the character has the piece of a set: an item of the type with the set's ancient type (A or B),
// worn or in the inventory.
bool HasSetPiece(const int type, const int ancientDiscriminator)
{
    const auto isPiece = [type, ancientDiscriminator](const ITEM* item)
    {
        return item != nullptr && item->Type == type && item->AncientDiscriminator == ancientDiscriminator;
    };

    if (CharacterMachine != nullptr)
    {
        for (int i = 0; i < MAX_EQUIPMENT_INDEX; ++i)
        {
            if (isPiece(&CharacterMachine->Equipment[i]))
            {
                return true;
            }
        }
    }

    if (g_pMyInventory != nullptr)
    {
        if (auto* inventory = g_pMyInventory->GetInventoryCtrl())
        {
            const auto count = static_cast<int>(inventory->GetNumberOfItems());
            for (int i = 0; i < count; ++i)
            {
                if (isPiece(inventory->GetItem(i)))
                {
                    return true;
                }
            }
        }
    }

    return false;
}
} // namespace

int CSItemOption::RenderPowerOfTheAncients(int textIndex)
{
    if (Hero == nullptr || CharacterMachine == nullptr)
    {
        return textIndex;
    }

    // The weapons the server's PowerOfTheAncientsPlugIn looks for.
    bool twoHandedSword = false;
    bool twoHandedStaff = false;
    bool bow = false;
    bool raven = false;
    for (const int slot : { EQUIPMENT_WEAPON_RIGHT, EQUIPMENT_WEAPON_LEFT, EQUIPMENT_HELPER })
    {
        const int type = CharacterMachine->Equipment[slot].Type;
        if (type < 0 || type >= MAX_ITEM)
        {
            continue;
        }

        const int group = type / MAX_ITEM_INDEX;
        const bool twoHanded = ItemAttribute[type].TwoHand;
        twoHandedSword |= group == ITEM_GROUP_SWORD && twoHanded;
        twoHandedStaff |= group == ITEM_GROUP_STAFF && twoHanded;
        bow |= group == ITEM_GROUP_BOW && type != ITEM_BOLT && type != ITEM_ARROWS;
        raven |= type == ITEM_DARK_RAVEN_ITEM;
    }

    struct Bonus
    {
        const wchar_t* format;
        int percent;
        bool weaponWorn;
    };

    std::vector<Bonus> bonuses;
    switch (gCharacterManager.GetBaseClass(Hero->Class))
    {
    case CLASS_KNIGHT:
        bonuses.push_back({ I18N::Game::PowerOfTheAncientsTwoHandedSwordD, 35, twoHandedSword });
        break;
    case CLASS_WIZARD:
        bonuses.push_back({ I18N::Game::PowerOfTheAncientsTwoHandedStaffD, 20, twoHandedStaff });
        break;
    case CLASS_ELF:
        bonuses.push_back({ I18N::Game::PowerOfTheAncientsBowD, 10, bow });
        break;
    case CLASS_DARK:
        bonuses.push_back({ I18N::Game::PowerOfTheAncientsTwoHandedSwordD, 40, twoHandedSword });
        bonuses.push_back({ I18N::Game::PowerOfTheAncientsTwoHandedStaffD, 25, twoHandedStaff });
        break;
    case CLASS_DARK_LORD:
        bonuses.push_back({ I18N::Game::PowerOfTheAncientsRavenD, 20, raven });
        break;
    default:
        return textIndex;
    }

    if (textIndex + static_cast<int>(bonuses.size()) + 6 > MAX_TEXT_LIST_LINES)
    {
        return textIndex;
    }

    // Hero->ExtendState is 1 while any ancient set is worn complete (calcSetOptionList).
    const bool fullSetWorn = Hero->ExtendState != 0;
    mu_swprintf(TextList[textIndex], L"%ls", I18N::Game::PowerOfTheAncients);
    TextListColor[textIndex] = TEXT_COLOR_YELLOW;
    TextBold[textIndex] = false;
    textIndex++;
    for (const auto& bonus : bonuses)
    {
        mu_swprintf(TextList[textIndex], bonus.format, bonus.percent);
        TextListColor[textIndex] = fullSetWorn && bonus.weaponWorn ? TEXT_COLOR_GREEN : TEXT_COLOR_GRAY;
        TextBold[textIndex] = false;
        textIndex++;
    }

    mu_swprintf(TextList[textIndex], L"\n");
    textIndex++;
    return textIndex;
}

int CSItemOption::RenderSetPieceList(const std::uint8_t setNumber, int textIndex) const
{
    struct Piece
    {
        int type;
        int ancientDiscriminator;
        bool owned;
    };

    std::vector<Piece> pieces;
    for (int type = 0; type < MAX_ITEM; ++type)
    {
        const auto& setType = m_ItemSetType[type];
        for (int k = 0; k < MAX_ITEM_SETS_PER_ITEM; ++k)
        {
            if (setType.byOption[k] == setNumber)
            {
                pieces.push_back({ type, k + 1, HasSetPiece(type, k + 1) });
                break;
            }
        }
    }

    // The header, the pieces and the closing lines of the tooltip must fit the text list.
    if (pieces.empty() || textIndex + static_cast<int>(pieces.size()) + 4 > MAX_TEXT_LIST_LINES)
    {
        return textIndex;
    }

    const auto owned = std::count_if(pieces.begin(), pieces.end(), [](const Piece& piece) { return piece.owned; });
    mu_swprintf(TextList[textIndex], I18N::Game::SetItemPieces, static_cast<int>(owned), static_cast<int>(pieces.size()));
    TextListColor[textIndex] = TEXT_COLOR_YELLOW;
    TextBold[textIndex] = false;
    textIndex++;

    for (const auto& piece : pieces)
    {
        // Named as the pieces themselves are ("Karis Helm").
        wchar_t name[MAX_ITEM_SET_NAME * 2]{};
        if (!GetAncientItemName(name, std::size(name), piece.type, piece.ancientDiscriminator))
        {
            mu_swprintf_s(name, L"%ls", ItemAttribute[piece.type].Name);
        }

        mu_swprintf(TextList[textIndex], L"%ls", name);
        TextListColor[textIndex] = piece.owned ? TEXT_COLOR_GREEN : TEXT_COLOR_GRAY;
        TextBold[textIndex] = false;
        textIndex++;
    }

    return textIndex;
}

std::uint8_t  CSItemOption::RenderSetOptionList(const SET_SEARCH_RESULT_OPT& set, std::uint8_t textIndex, bool bIsEquippedItem, bool bShowInactive)
{
    for (int j = 0; j < set.SetOptionCount; j++)
    {
        const auto option = set.SetOption[j];
        if (!bShowInactive && !option.IsActive)
        {
            break;
        }

        if (getExplainText(TextList[textIndex], option.OptionNumber, option.Value))
        {
            if (!bIsEquippedItem || !option.IsActive)
            {
                TextListColor[textIndex] = TEXT_COLOR_GRAY;
            }
            else if (option.OptionNumber >= AT_SET_OPTION_IMPROVE_ATTACK_1 && !option.FulfillsClassRequirement)
            {
                // Mastery
                TextListColor[textIndex] = TEXT_COLOR_RED;
            }
            else
            {
                TextListColor[textIndex] = option.IsFullOption ? TEXT_COLOR_YELLOW
                                            : option.IsExtOption ? TEXT_COLOR_GREEN
                                            : TEXT_COLOR_BLUE;
            }

            TextBold[textIndex] = false;
            textIndex++;
        }
    }

    mu_swprintf(TextList[textIndex], L"\n");
    textIndex++;

    return textIndex;
}

void CSItemOption::SetViewOptionList(bool bView)
{
    m_bViewOptionList = bView;
}

bool CSItemOption::IsViewOptionList()
{
    return m_bViewOptionList;
}
