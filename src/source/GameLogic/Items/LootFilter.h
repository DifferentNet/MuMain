#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The loot highlight filter (ghinzuro 2026-11-11: «фильтр подсвечивания лута, по типу лута, по редкости»).
// Client only: each ground item gets one verdict, Hide, Normal or Highlight, when it drops. A highlighted item keeps
// its label on screen and gets a short beam; a hidden one draws no label and no shine (it still drops, shows on hover
// and is picked up as before). This file holds the pure rules; LootFilterRuntime.cpp reads the item and the config.
namespace GameLogic::Items::LootFilter
{
// Zero is Normal, so an item slot that was never classified behaves as before the filter.
enum class Verdict : std::uint8_t
{
    Normal = 0,
    Hide = 1,
    Highlight = 2,
};

// One per item, the first match wins (in this order).
enum class Category : std::uint8_t
{
    Zen,
    Jewel,
    Wings,
    Container,
    Quest,
    SocketSeed,
    MedalTicket,
    Consumable,
    Weapon,
    Shield,
    Armour,
    Jewellery,
    Other,
    Count,
};

inline constexpr int CategoryCount = static_cast<int>(Category::Count);

// Rarity flags of one item; the settings' rarity mask highlights any item that has one of the chosen flags.
enum RarityFlag : std::uint8_t
{
    RarityExcellent = 1 << 0,
    RarityAncient = 1 << 1,
    RaritySocket = 1 << 2,
    RarityLuck = 1 << 3,
    RaritySkill = 1 << 4,
    RarityOption = 1 << 5,
    RarityLevel = 1 << 6, // the item's level is at least Settings::minLevel
};

inline constexpr int RarityFlagCount = 7;
inline constexpr std::uint8_t RarityAllMask = (1 << RarityFlagCount) - 1;

// What the rules need to know about one dropped item.
struct Facts
{
    Category category = Category::Other;
    std::uint8_t rarity = 0; // RarityFlag bits, without RarityLevel (Classify adds it from level)
    int level = 0;
    std::uint32_t zenAmount = 0; // only for Category::Zen
    std::wstring lowerName; // the shown name, folded with FoldCase
};

inline constexpr int DefaultMinLevel = 9;
inline constexpr int MinLevelLow = 0;
inline constexpr int MinLevelHigh = 15;
inline constexpr std::uint32_t ZenMinHigh = 2'000'000'000;
inline constexpr std::size_t MaxNames = 32;
inline constexpr std::size_t MaxNameLength = 40;

struct Settings
{
    bool enabled = true;
    bool marker = true; // the beam over highlighted items
    bool sound = false; // a short sound when a highlighted item drops
    std::array<Verdict, CategoryCount> categories{};
    std::uint8_t rarity = 0; // RarityFlag bits that highlight
    int minLevel = DefaultMinLevel;
    std::uint32_t zenMin = 0; // 0 = off; a zen pile of at least this much is highlighted
    std::vector<std::wstring> names; // folded substrings; a match highlights
};

enum class Preset : std::uint8_t
{
    Off,
    Default,
    Strict,
};

// Hiding is never allowed for these categories (jewels and quest items stay visible).
[[nodiscard]] bool CanHide(Category category);

// The verdict for one item under the settings; see the order in LootFilter.cpp.
[[nodiscard]] Verdict Classify(const Settings& settings, const Facts& facts);

// The category actions, the rarity mask and the thresholds of a preset; enabled, marker, sound and names are kept.
void ApplyPreset(Settings& settings, Preset preset);
[[nodiscard]] Settings MakePreset(Preset preset);

// The category actions as one digit per category ("0" Normal, "1" Hide, "2" Highlight), in Category order.
[[nodiscard]] std::wstring SerializeCategories(const std::array<Verdict, CategoryCount>& categories);

// Reads SerializeCategories' string into categories; false (and categories untouched) when it has the wrong length
// or a bad digit. A Hide for a category that cannot hide becomes Normal.
[[nodiscard]] bool ParseCategories(std::wstring_view text, std::array<Verdict, CategoryCount>& categories);

// Lower case for Latin, Latin-1 and Cyrillic letters, so names match whatever the client language.
[[nodiscard]] std::wstring FoldCase(std::wstring_view text);

// The name trimmed, folded and cut to MaxNameLength; empty when nothing is left.
[[nodiscard]] std::wstring NormalizeName(std::wstring_view text);
} // namespace GameLogic::Items::LootFilter
