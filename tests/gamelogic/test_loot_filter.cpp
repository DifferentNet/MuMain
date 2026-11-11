#include "doctest.h"

#include <string>

#include "Data/GameConfig/GameConfigConstants.h"
#include "GameLogic/Items/LootFilter.h"

using namespace GameLogic::Items::LootFilter;

namespace
{
Facts Item(Category category, std::uint8_t rarity = 0, int level = 0)
{
    Facts facts;
    facts.category = category;
    facts.rarity = rarity;
    facts.level = level;
    return facts;
}

Facts Zen(std::uint32_t amount)
{
    Facts facts;
    facts.category = Category::Zen;
    facts.zenAmount = amount;
    return facts;
}

void Set(Settings& settings, Category category, Verdict verdict)
{
    settings.categories[static_cast<std::size_t>(category)] = verdict;
}
} // namespace

TEST_CASE("Zero is Normal, so an unclassified item draws as before")
{
    CHECK(static_cast<int>(Verdict::Normal) == 0);
}

TEST_CASE("The Default preset highlights jewels, wings, boxes, medals, excellent and ancient")
{
    const Settings settings = MakePreset(Preset::Default);
    CHECK(Classify(settings, Item(Category::Jewel)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Wings)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Container)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::MedalTicket)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Armour, RarityExcellent)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Weapon, RarityAncient)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Armour)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Consumable)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Weapon, RarityLuck, 13)) == Verdict::Normal);
    CHECK(Classify(settings, Zen(1'000'000)) == Verdict::Normal);
    for (const Verdict verdict : settings.categories)
        CHECK(verdict != Verdict::Hide);
}

TEST_CASE("The Off preset leaves everything Normal")
{
    const Settings settings = MakePreset(Preset::Off);
    CHECK(Classify(settings, Item(Category::Jewel)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Armour, RarityExcellent)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Consumable)) == Verdict::Normal);
}

TEST_CASE("The Strict preset hides plain gear and potions but never excellent or ancient")
{
    const Settings settings = MakePreset(Preset::Strict);
    CHECK(Classify(settings, Item(Category::Consumable)) == Verdict::Hide);
    CHECK(Classify(settings, Item(Category::Armour, 0, 3)) == Verdict::Hide);
    CHECK(Classify(settings, Item(Category::Armour, RarityExcellent)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Armour, 0, 11)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Jewel)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::MedalTicket)) == Verdict::Highlight);
}

TEST_CASE("Sockets are retired: the Strict preset does not highlight them")
{
    // No socket items or seeds on the server (ghinzuro 2026-11-11); the enum slot and the bit stay for saved settings.
    const Settings settings = MakePreset(Preset::Strict);
    CHECK(IsRetired(Category::SocketSeed));
    CHECK_FALSE(IsRetired(Category::Weapon));
    CHECK(settings.categories[static_cast<std::size_t>(Category::SocketSeed)] == Verdict::Normal);
    CHECK((settings.rarity & RarityRetiredMask) == 0);
    CHECK(Classify(settings, Item(Category::SocketSeed)) == Verdict::Normal);
    // A socket bit no longer counts, so it is a plain weapon, which Strict hides.
    CHECK(Classify(settings, Item(Category::Weapon, RaritySocket)) == Verdict::Hide);
}

TEST_CASE("Saved socket settings have no effect")
{
    Settings settings = MakePreset(Preset::Off);
    settings.rarity = RarityAllMask;
    settings.minLevel = MinLevelHigh;
    Set(settings, Category::SocketSeed, Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::SocketSeed)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Weapon, RaritySocket)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Weapon, RarityLuck)) == Verdict::Highlight);
}

TEST_CASE("The retired socket category reads as Normal and saves as 0")
{
    std::wstring text(static_cast<std::size_t>(CategoryCount), L'0');
    const auto socket = static_cast<std::size_t>(Category::SocketSeed);
    text[socket] = L'2';
    std::array<Verdict, CategoryCount> categories{};
    REQUIRE(ParseCategories(text, categories));
    CHECK(categories[socket] == Verdict::Normal);

    categories[socket] = Verdict::Highlight;
    const std::wstring saved = SerializeCategories(categories);
    CHECK(saved.size() == static_cast<std::size_t>(CategoryCount));
    CHECK(saved[socket] == L'0');
}

TEST_CASE("A disabled filter gives Normal for everything")
{
    Settings settings = MakePreset(Preset::Strict);
    settings.enabled = false;
    settings.names = {L"bless"};
    CHECK(Classify(settings, Item(Category::Consumable)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Jewel)) == Verdict::Normal);
}

TEST_CASE("A name match wins over a Hide category")
{
    Settings settings = MakePreset(Preset::Off);
    Set(settings, Category::Consumable, Verdict::Hide);
    settings.names = {FoldCase(L"Elixir")};
    Facts facts = Item(Category::Consumable);
    facts.lowerName = FoldCase(L"Small Elixir of Life");
    CHECK(Classify(settings, facts) == Verdict::Highlight);
    facts.lowerName = FoldCase(L"Healing Potion");
    CHECK(Classify(settings, facts) == Verdict::Hide);
}

TEST_CASE("Names match Cyrillic in any case")
{
    Settings settings = MakePreset(Preset::Off);
    settings.names = {NormalizeName(L"  ЗЕЛЬЕ ")};
    Facts facts = Item(Category::Consumable);
    facts.lowerName = FoldCase(L"Большое Зелье лечения");
    CHECK(Classify(settings, facts) == Verdict::Highlight);
    CHECK(FoldCase(L"ЁЇІҐ") == L"ёїіґ");
}

TEST_CASE("A chosen rarity wins over a Hide category")
{
    Settings settings = MakePreset(Preset::Off);
    Set(settings, Category::Weapon, Verdict::Hide);
    settings.rarity = RarityLuck;
    CHECK(Classify(settings, Item(Category::Weapon, RarityLuck)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Weapon, RaritySkill)) == Verdict::Hide);
}

TEST_CASE("The level threshold highlights from minLevel up")
{
    Settings settings = MakePreset(Preset::Off);
    settings.rarity = RarityLevel;
    settings.minLevel = 9;
    CHECK(Classify(settings, Item(Category::Armour, 0, 8)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Armour, 0, 9)) == Verdict::Highlight);
    CHECK(Classify(settings, Item(Category::Armour, 0, 15)) == Verdict::Highlight);
}

TEST_CASE("The zen threshold highlights big piles and is off at zero")
{
    Settings settings = MakePreset(Preset::Off);
    CHECK(Classify(settings, Zen(5'000'000)) == Verdict::Normal);
    settings.zenMin = 100'000;
    CHECK(Classify(settings, Zen(99'999)) == Verdict::Normal);
    CHECK(Classify(settings, Zen(100'000)) == Verdict::Highlight);
    Set(settings, Category::Zen, Verdict::Hide);
    CHECK(Classify(settings, Zen(10)) == Verdict::Hide);
    CHECK(Classify(settings, Zen(200'000)) == Verdict::Highlight);
}

TEST_CASE("Jewels and quest items are never hidden")
{
    Settings settings = MakePreset(Preset::Off);
    Set(settings, Category::Jewel, Verdict::Hide);
    Set(settings, Category::Quest, Verdict::Hide);
    CHECK_FALSE(CanHide(Category::Jewel));
    CHECK_FALSE(CanHide(Category::Quest));
    CHECK(CanHide(Category::Consumable));
    CHECK(Classify(settings, Item(Category::Jewel)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Quest)) == Verdict::Normal);
}

TEST_CASE("Excellent and ancient items are never hidden, even without their rarity chosen")
{
    Settings settings = MakePreset(Preset::Off);
    Set(settings, Category::Armour, Verdict::Hide);
    CHECK(Classify(settings, Item(Category::Armour, RarityExcellent)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Armour, RarityAncient)) == Verdict::Normal);
    CHECK(Classify(settings, Item(Category::Armour, RarityLuck)) == Verdict::Hide);
}

TEST_CASE("The category string round-trips")
{
    Settings settings = MakePreset(Preset::Strict);
    const std::wstring text = SerializeCategories(settings.categories);
    CHECK(text.size() == static_cast<std::size_t>(CategoryCount));

    std::array<Verdict, CategoryCount> parsed{};
    REQUIRE(ParseCategories(text, parsed));
    CHECK(parsed == settings.categories);
}

TEST_CASE("A bad category string leaves the categories untouched")
{
    std::array<Verdict, CategoryCount> categories = MakePreset(Preset::Default).categories;
    const auto before = categories;
    CHECK_FALSE(ParseCategories(L"", categories));
    CHECK_FALSE(ParseCategories(L"0000000000000000", categories));
    CHECK_FALSE(ParseCategories(L"00000000000x0", categories));
    CHECK(categories == before);
}

TEST_CASE("A saved Hide for jewels or quest items loads as Normal")
{
    std::array<Verdict, CategoryCount> categories{};
    REQUIRE(ParseCategories(L"1111111111111", categories));
    CHECK(categories[static_cast<std::size_t>(Category::Jewel)] == Verdict::Normal);
    CHECK(categories[static_cast<std::size_t>(Category::Quest)] == Verdict::Normal);
    CHECK(categories[static_cast<std::size_t>(Category::Zen)] == Verdict::Hide);
}

TEST_CASE("Names are trimmed, folded and cut")
{
    CHECK(NormalizeName(L"   ").empty());
    CHECK(NormalizeName(L" Jewel of Bless ") == L"jewel of bless");
    CHECK(NormalizeName(std::wstring(100, L'A')).size() == MaxNameLength);
}

TEST_CASE("The config defaults match the Default preset")
{
    const Settings preset = MakePreset(Preset::Default);
    CHECK(CfgDefaults::CfgDefaultLootFilterRarity == preset.rarity);
    CHECK(CfgDefaults::CfgDefaultLootFilterMinLevel == preset.minLevel);
    CHECK(CfgDefaults::CfgDefaultLootFilterZenMin == static_cast<int>(preset.zenMin));
    CHECK(CfgDefaults::CfgDefaultLootFilterEnabled == preset.enabled);
    CHECK(CfgDefaults::CfgDefaultLootFilterMarker == preset.marker);
    CHECK(CfgDefaults::CfgDefaultLootFilterSound == preset.sound);
    CHECK(static_cast<std::size_t>(CfgDefaults::CfgMaxLootFilterNames) == MaxNames);
}
