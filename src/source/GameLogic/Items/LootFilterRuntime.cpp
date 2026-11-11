#include "stdafx.h"
#include "GameLogic/Items/LootFilterRuntime.h"

#include <algorithm>
#include <chrono>
#include <cwchar>

#include "Audio/DSPlaySound.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Data/GameData/ItemData/ItemDefinition.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzObject.h"
#include "GameLogic/Items/CComGem.h"
#include "GameLogic/Items/ItemCategories.h"
#include "Network/Server/SocketSystem.h"

namespace GameLogic::Items::LootFilterRuntime
{
namespace
{
using LootFilter::Category;
using LootFilter::Facts;
using LootFilter::Settings;
using LootFilter::Verdict;

// At most one highlight sound in this time, so a boss's pile of drops rings once.
constexpr auto SoundGap = std::chrono::milliseconds(300);

Settings g_settings;
bool g_loaded = false;
std::chrono::steady_clock::time_point g_lastSound{};

Settings LoadFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    Settings settings = LootFilter::MakePreset(LootFilter::Preset::Default);
    settings.enabled = config.GetLootFilterEnabled();
    settings.marker = config.GetLootFilterMarker();
    settings.sound = config.GetLootFilterSound();
    // An empty or broken string keeps the Default preset's categories.
    (void)LootFilter::ParseCategories(config.GetLootFilterCategories(), settings.categories);
    settings.rarity = static_cast<std::uint8_t>(config.GetLootFilterRarity() & LootFilter::RarityAllMask);
    settings.minLevel =
        std::clamp(config.GetLootFilterMinLevel(), LootFilter::MinLevelLow, LootFilter::MinLevelHigh);
    settings.zenMin = static_cast<std::uint32_t>(
        std::clamp<long long>(config.GetLootFilterZenMin(), 0, LootFilter::ZenMinHigh));

    for (const std::wstring& name : config.GetLootFilterNames())
    {
        std::wstring normalized = LootFilter::NormalizeName(name);
        if (!normalized.empty() && settings.names.size() < LootFilter::MaxNames)
            settings.names.push_back(std::move(normalized));
    }
    return settings;
}

void SaveToConfig(const Settings& settings)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetLootFilterEnabled(settings.enabled);
    config.SetLootFilterMarker(settings.marker);
    config.SetLootFilterSound(settings.sound);
    config.SetLootFilterCategories(LootFilter::SerializeCategories(settings.categories));
    config.SetLootFilterRarity(settings.rarity);
    config.SetLootFilterMinLevel(settings.minLevel);
    config.SetLootFilterZenMin(static_cast<int>(settings.zenMin));
    config.SetLootFilterNames(settings.names);
    config.Save();
}

void EnsureLoaded()
{
    if (g_loaded)
        return;
    g_settings = LoadFromConfig();
    g_loaded = true;
}

bool IsConsumable(const ITEM& item)
{
    const int type = item.Type;
    return IsHealingPotion(&item) || IsManaPotionType(type) || IsShieldPotionType(type) || IsComplexPotionType(type)
        || IsElitePotion(&item) || IsElixir(&item) || IsBuffScroll(&item) || IsAmmunition(&item);
}

bool IsMedalOrTicket(const ITEM& item)
{
    return IsTieredMedal(&item) || IsEventTicket(&item) || IsLuckyItemTicket(&item)
        || IsBloodCastleTicketPart(&item);
}

Category GearCategory(const ITEM& item)
{
    const int group = item.Type / MAX_ITEM_INDEX;
    if (IsShieldType(item.Type) || group == ITEM_GROUP_SHIELD)
        return Category::Shield;
    if (group >= ITEM_GROUP_SWORD && group <= ITEM_GROUP_STAFF)
        return Category::Weapon;
    if (group >= ITEM_GROUP_HELM && group <= ITEM_GROUP_BOOTS)
        return Category::Armour;

    const Data::Items::ItemSlot slot = g_ItemDatabase.GetSlot(item.Type);
    if (slot == Data::Items::ItemSlot::Ring || slot == Data::Items::ItemSlot::Pendant)
        return Category::Jewellery;
    return Category::Other;
}

Category CategoryOf(const ITEM& item)
{
    ITEM* mutableItem = const_cast<ITEM*>(&item); // the old helpers take a non-const pointer
    if (IsMoneyItem(mutableItem))
        return Category::Zen;
    // A bundle of jewels (10 Bless and so on) is a jewel too.
    if (IsJewelItem(&item) || COMGEM::isCompiledGem(&item))
        return Category::Jewel;
    if (IsWingItem(&item))
        return Category::Wings;
    // Before the containers: IsContainerItem counts the tiered medals as boxes.
    if (IsTieredMedal(&item))
        return Category::MedalTicket;
    if (IsContainerItem(&item))
        return Category::Container;
    if (IsQuestItem(&item))
        return Category::Quest;
    if (IsSocketSeedOrSphere(&item))
        return Category::SocketSeed;
    if (IsMedalOrTicket(item))
        return Category::MedalTicket;
    if (IsConsumable(item))
        return Category::Consumable;
    return GearCategory(item);
}

std::uint8_t RarityOf(const ITEM& item)
{
    std::uint8_t rarity = 0;
    if (IsExcellent(item))
        rarity |= LootFilter::RarityExcellent;
    if (item.AncientDiscriminator != 0)
        rarity |= LootFilter::RarityAncient;
    if (g_SocketItemMgr.IsSocketItem(&item))
        rarity |= LootFilter::RaritySocket;
    if (item.HasLuck)
        rarity |= LootFilter::RarityLuck;
    if (item.HasSkill)
        rarity |= LootFilter::RaritySkill;
    if (item.OptionLevel != 0)
        rarity |= LootFilter::RarityOption;
    return rarity;
}

bool ValidIndex(int index)
{
    return index >= 0 && index < MAX_ITEMS;
}

void PlayHighlightSound()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - g_lastSound < SoundGap)
        return;
    g_lastSound = now;
    PlayBuffer(SOUND_MEDAL);
}
} // namespace

const Settings& Current()
{
    EnsureLoaded();
    return g_settings;
}

void Apply(const Settings& settings)
{
    g_settings = settings;
    g_loaded = true;
    SaveToConfig(g_settings);
    ReclassifyAll();
}

bool IsExcellent(const ITEM& item)
{
    // The same test as the green label colour (BuildGroundItemLabelDescriptor).
    return (item.ExcellentFlags & 63) != 0 && !IsWingItem(&item) && item.Type != ITEM_HORN_OF_FENRIR;
}

Facts BuildFacts(OBJECT& object, ITEM& item)
{
    Facts facts;
    facts.category = CategoryOf(item);
    if (facts.category == Category::Zen)
    {
        // A zen pile keeps its amount in Level (CreateMoneyDrop).
        facts.zenAmount = static_cast<std::uint32_t>(std::max(item.Level, 0));
        return facts;
    }
    facts.rarity = RarityOf(item);
    facts.level = item.Level;
    if (ItemAttribute != nullptr && item.Type >= 0 && item.Type < MAX_ITEM)
        facts.lowerName = LootFilter::FoldCase(GroundItemLabelText(&object, &item));
    return facts;
}

void ClassifyDrop(int index, bool isFreshDrop)
{
    if (!ValidIndex(index))
        return;
    ITEM_t& entry = ::Items[index];
    const Verdict verdict = LootFilter::Classify(Current(), BuildFacts(entry.Object, entry.Item));
    entry.LootVerdict = static_cast<BYTE>(verdict);
    if (isFreshDrop && verdict == Verdict::Highlight && g_settings.sound)
        PlayHighlightSound();
}

void ReclassifyAll()
{
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        if (::Items[i].Object.Live)
            ClassifyDrop(i, false);
    }
}

Verdict VerdictOf(int index)
{
    if (!ValidIndex(index))
        return Verdict::Normal;
    return static_cast<Verdict>(::Items[index].LootVerdict);
}

bool IsHidden(int index)
{
    return VerdictOf(index) == Verdict::Hide;
}

bool IsHighlighted(int index)
{
    return VerdictOf(index) == Verdict::Highlight;
}

bool MarkerOn()
{
    const Settings& settings = Current();
    return settings.enabled && settings.marker;
}
} // namespace GameLogic::Items::LootFilterRuntime
