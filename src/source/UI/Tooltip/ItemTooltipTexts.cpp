#include "stdafx.h"

#include "UI/Tooltip/ItemTooltipTexts.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"

namespace UI::Tooltip
{
namespace
{
using GameLogic::Items::LootFilter::Category;
using GameLogic::Items::Rarity::Tier;

constexpr auto CategoryCount = static_cast<std::size_t>(GameLogic::Items::LootFilter::CategoryCount);

// The kind line's rarity prefix.
enum class KindPrefix : std::uint8_t
{
    None,
    Excellent,
    Ancient,
    Count,
};
constexpr auto KindPrefixCount = static_cast<std::size_t>(KindPrefix::Count);

// The loot filter's name of the item's type; nullptr for zen and other items (no kind line).
const wchar_t* ItemKindWord(Category category)
{
    switch (category)
    {
    case Category::Jewel:
        return I18N::Game::LootFilterCatJewel;
    case Category::Wings:
        return I18N::Game::LootFilterCatWings;
    case Category::Container:
        return I18N::Game::LootFilterCatContainer;
    case Category::Quest:
        return I18N::Game::LootFilterCatQuest;
    case Category::MedalTicket:
        return I18N::Game::LootFilterCatMedal;
    case Category::Consumable:
        return I18N::Game::LootFilterCatConsumable;
    case Category::Weapon:
        return I18N::Game::LootFilterCatWeapon;
    case Category::Shield:
        return I18N::Game::LootFilterCatShield;
    case Category::Armour:
        return I18N::Game::LootFilterCatArmour;
    case Category::Jewellery:
        return I18N::Game::LootFilterCatJewellery;
    default:
        return nullptr;
    }
}

// The loot filter's name of the rarity; nullptr for no prefix.
const wchar_t* PrefixWord(KindPrefix prefix)
{
    switch (prefix)
    {
    case KindPrefix::Excellent:
        return I18N::Game::LootFilterRarExcellent;
    case KindPrefix::Ancient:
        return I18N::Game::LootFilterRarAncient;
    default:
        return nullptr;
    }
}

KindPrefix PrefixOf(Tier rarity)
{
    if (rarity == Tier::Ancient)
        return KindPrefix::Ancient;
    if (rarity == Tier::Excellent)
        return KindPrefix::Excellent;
    return KindPrefix::None;
}

std::string BuildKindText(Category category, KindPrefix prefix)
{
    const wchar_t* word = ItemKindWord(category);
    if (word == nullptr)
        return {};

    constexpr const wchar_t* Separator = L" · ";
    std::wstring text;
    if (const wchar_t* rarity = PrefixWord(prefix); rarity != nullptr)
        text.append(rarity).append(Separator);
    text.append(word);
    return StringUtils::WideToNarrow(text.c_str());
}

// Every text of one language; `locale` is I18N's own pointer for it, the same for as long as that language is on.
struct Texts
{
    const char* locale = nullptr;
    std::string hint;
    std::array<std::array<std::string, KindPrefixCount>, CategoryCount> kinds;
};

const Texts& CurrentTexts()
{
    static Texts texts;
    const char* locale = I18N::GetCurrentLocale();
    if (texts.locale == locale)
        return texts;

    texts.locale = locale;
    texts.hint = StringUtils::WideToNarrow(I18N::Game::TooltipShiftForDetails);
    for (std::size_t category = 0; category < CategoryCount; ++category)
    {
        for (std::size_t prefix = 0; prefix < KindPrefixCount; ++prefix)
            texts.kinds[category][prefix] =
                BuildKindText(static_cast<Category>(category), static_cast<KindPrefix>(prefix));
    }
    return texts;
}
} // namespace

std::string_view ItemKindText(Category category, Tier rarity)
{
    const auto index = static_cast<std::size_t>(category);
    if (index >= CategoryCount)
        return {};
    return CurrentTexts().kinds[index][static_cast<std::size_t>(PrefixOf(rarity))];
}

std::string_view ShiftForDetailsHint()
{
    return CurrentTexts().hint;
}
} // namespace UI::Tooltip
