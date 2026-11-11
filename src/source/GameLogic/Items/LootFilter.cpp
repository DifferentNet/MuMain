#include "GameLogic/Items/LootFilter.h"

#include <cwctype>

namespace GameLogic::Items::LootFilter
{
namespace
{
constexpr wchar_t DigitNormal = L'0';
constexpr wchar_t DigitHide = L'1';
constexpr wchar_t DigitHighlight = L'2';

// Excellent and ancient items are never hidden (ghinzuro 2026-11-11).
constexpr std::uint8_t NeverHiddenRarity = RarityExcellent | RarityAncient;

constexpr std::size_t Index(Category category)
{
    return static_cast<std::size_t>(category);
}

bool NameMatches(const std::vector<std::wstring>& names, const std::wstring& lowerName)
{
    if (lowerName.empty())
        return false;
    for (const std::wstring& name : names)
    {
        if (!name.empty() && lowerName.find(name) != std::wstring::npos)
            return true;
    }
    return false;
}

std::uint8_t RarityOf(const Settings& settings, const Facts& facts)
{
    std::uint8_t rarity = facts.rarity;
    if (facts.category != Category::Zen && facts.level >= settings.minLevel)
        rarity |= RarityLevel;
    return rarity;
}

Verdict ClampHide(Verdict verdict, const Facts& facts)
{
    if (verdict != Verdict::Hide)
        return verdict;
    if (!CanHide(facts.category) || (facts.rarity & NeverHiddenRarity) != 0)
        return Verdict::Normal;
    return Verdict::Hide;
}

wchar_t FoldChar(wchar_t c)
{
    if (c >= L'A' && c <= L'Z')
        return static_cast<wchar_t>(c + (L'a' - L'A'));
    if (c >= 0x0410 && c <= 0x042F) // Cyrillic А..Я
        return static_cast<wchar_t>(c + 0x20);
    if (c >= 0x0400 && c <= 0x040F) // Cyrillic Ѐ..Џ (Ё, Є, І, Ї, Ў ...)
        return static_cast<wchar_t>(c + 0x50);
    if (c == 0x0490) // Ukrainian Ґ
        return 0x0491;
    return static_cast<wchar_t>(std::towlower(c));
}

bool IsSpace(wchar_t c)
{
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
}
} // namespace

bool CanHide(Category category)
{
    return category != Category::Jewel && category != Category::Quest;
}

Verdict Classify(const Settings& settings, const Facts& facts)
{
    if (!settings.enabled)
        return Verdict::Normal;
    if (NameMatches(settings.names, facts.lowerName))
        return Verdict::Highlight;
    if ((RarityOf(settings, facts) & settings.rarity) != 0)
        return Verdict::Highlight;
    if (facts.category == Category::Zen && settings.zenMin > 0 && facts.zenAmount >= settings.zenMin)
        return Verdict::Highlight;

    return ClampHide(settings.categories[Index(facts.category)], facts);
}

void ApplyPreset(Settings& settings, Preset preset)
{
    settings.categories.fill(Verdict::Normal);
    settings.rarity = 0;
    settings.minLevel = DefaultMinLevel;
    settings.zenMin = 0;
    if (preset == Preset::Off)
        return;

    // «Ценное»: jewels, wings, boxes, excellent and ancient (ghinzuro 2026-11-11); medals and tickets too, as the
    // medals used to count as boxes.
    settings.categories[Index(Category::Jewel)] = Verdict::Highlight;
    settings.categories[Index(Category::Wings)] = Verdict::Highlight;
    settings.categories[Index(Category::Container)] = Verdict::Highlight;
    settings.categories[Index(Category::MedalTicket)] = Verdict::Highlight;
    settings.rarity = RarityExcellent | RarityAncient;
    if (preset == Preset::Default)
        return;

    // Strict: also seeds and socket items and high levels; plain gear and potions are hidden.
    settings.categories[Index(Category::SocketSeed)] = Verdict::Highlight;
    settings.rarity |= RaritySocket | RarityLevel;
    settings.categories[Index(Category::Consumable)] = Verdict::Hide;
    settings.categories[Index(Category::Weapon)] = Verdict::Hide;
    settings.categories[Index(Category::Shield)] = Verdict::Hide;
    settings.categories[Index(Category::Armour)] = Verdict::Hide;
    settings.categories[Index(Category::Jewellery)] = Verdict::Hide;
}

Settings MakePreset(Preset preset)
{
    Settings settings;
    ApplyPreset(settings, preset);
    return settings;
}

std::wstring SerializeCategories(const std::array<Verdict, CategoryCount>& categories)
{
    std::wstring text;
    text.reserve(CategoryCount);
    for (const Verdict verdict : categories)
    {
        if (verdict == Verdict::Hide)
            text.push_back(DigitHide);
        else if (verdict == Verdict::Highlight)
            text.push_back(DigitHighlight);
        else
            text.push_back(DigitNormal);
    }
    return text;
}

bool ParseCategories(std::wstring_view text, std::array<Verdict, CategoryCount>& categories)
{
    if (text.size() != static_cast<std::size_t>(CategoryCount))
        return false;

    std::array<Verdict, CategoryCount> parsed{};
    for (int i = 0; i < CategoryCount; ++i)
    {
        const wchar_t c = text[static_cast<std::size_t>(i)];
        if (c == DigitNormal)
            parsed[i] = Verdict::Normal;
        else if (c == DigitHide)
            parsed[i] = CanHide(static_cast<Category>(i)) ? Verdict::Hide : Verdict::Normal;
        else if (c == DigitHighlight)
            parsed[i] = Verdict::Highlight;
        else
            return false;
    }
    categories = parsed;
    return true;
}

std::wstring FoldCase(std::wstring_view text)
{
    std::wstring folded;
    folded.reserve(text.size());
    for (const wchar_t c : text)
        folded.push_back(FoldChar(c));
    return folded;
}

std::wstring NormalizeName(std::wstring_view text)
{
    while (!text.empty() && IsSpace(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && IsSpace(text.back()))
        text.remove_suffix(1);
    if (text.size() > MaxNameLength)
        text = text.substr(0, MaxNameLength);
    return FoldCase(text);
}
} // namespace GameLogic::Items::LootFilter
