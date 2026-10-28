#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// MU Moldova: titles over the names (ghinzuro, 2026-10-27). A character wears at most one title; others see it as
// a plate over its name (UI/Character/TitlePlates.cpp), the titles window (UI/Events/TitlesWindow.cpp) lists the
// own ones and the catalogue.
//
// Our server's add-on (MU Moldova, Titles) answers hidden chat commands with blue server messages, which this takes
// out of the chat:
//   "/titlesask"        -> "#TTL|name|title|rarity|English|Russian" for every character in the game that wears one;
//                          from then on one for each character that enters the world or changes its title. An empty
//                          title ("#TTL|name||||") takes the plate away.
//   "/titleslist en|ru" -> "#TTW0|worn|auto" (auto 1: the character never chose, a new title goes on by itself),
//                          "#TTW|title|rarity|term|owned|seconds left|name|hint|bonus" for the own titles and then
//                          the catalogue's, "#TTW.|" at the end. Seconds left: 0 forever, -1 while held. Bonus: what
//                          the worn title of that rarity gives, "HP;taken;damage" in percent ("3;1;1"), empty: none.
//   "/title <title>" or "/title off" wears a title or none; the server answers with the new "#TTL" line.
namespace GameLogic::Titles
{
// The plate's colour: how rare the title is (the server's TitleRarity).
enum class Rarity : int
{
    Silver = 0,
    Ice = 1,
    Jade = 2,
    Gold = 3,
    Crimson = 4,
    Violet = 5,
};

inline constexpr int RarityCount = 6;

// How long a title lasts (the server's TitleTerm).
enum class Term : int
{
    Forever = 0,
    Week = 1,
    Season = 2,
    WhileHeld = 3,
};

// The title a character wears.
struct Worn
{
    std::wstring Id;
    Rarity Color = Rarity::Silver;
    std::wstring English;
    std::wstring Russian;

    // The name in the client's language.
    const std::wstring& Text() const;
};

// A line of the titles window.
struct Entry
{
    std::wstring Id;
    Rarity Color = Rarity::Silver;
    Term Lasts = Term::Forever;
    bool Owned = false;
    // Seconds left when the list came: 0 forever, -1 while held.
    long long SecondsLeft = 0;
    std::wstring Name;
    std::wstring Hint;
    // The bonus while it is worn, in percent: more maximum health, less damage taken, more damage dealt.
    float BonusHealth = 0.f;
    float BonusTaken = 0.f;
    float BonusDamage = 0.f;

    bool HasBonus() const { return BonusHealth != 0.f || BonusTaken != 0.f || BonusDamage != 0.f; }
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was a titles line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the titles of the characters in the game ("/titlesask").
    void Request();

    // Asks for the titles window's list in the client's language ("/titleslist en|ru").
    void RequestList();

    // Wears a title ("/title <id>"), or none for an empty id ("/title off"), and asks for the list again.
    void Wear(std::wstring_view id);

    // Forgets everything, e.g. when the character changed; the next Request() brings the plates again.
    void Reset();

    // The title a character wears (by its name as the game shows it), or nullptr.
    const Worn* Of(std::wstring_view name) const;

    // The titles window's list, its own titles first; ListVersion() changes with every new list.
    const std::vector<Entry>& List() const { return m_list; }
    const std::wstring& Wearing() const { return m_wearing; }
    bool Automatic() const { return m_automatic; }
    bool HasList() const { return m_hasList; }
    unsigned ListVersion() const { return m_listVersion; }
    // When the list came (GetTickCount64), for the time left.
    std::uint64_t ListTime() const { return m_listTime; }

    // How many characters are known to wear a title.
    size_t Count() const { return m_worn.size(); }

    // True while the client shows Russian.
    static bool Russian();

private:
    struct NameHash
    {
        using is_transparent = void;
        size_t operator()(std::wstring_view name) const noexcept { return std::hash<std::wstring_view>{}(name); }
    };

    void TakeWorn(const std::vector<std::wstring>& fields);
    void TakeListLine(std::wstring_view tag, const std::vector<std::wstring>& fields);

    std::unordered_map<std::wstring, Worn, NameHash, std::equal_to<>> m_worn;

    std::vector<Entry> m_list;
    std::vector<Entry> m_incoming;
    std::wstring m_wearing;
    std::wstring m_incomingWearing;
    bool m_automatic = false;
    bool m_incomingAutomatic = false;
    bool m_hasList = false;
    unsigned m_listVersion = 0;
    std::uint64_t m_listTime = 0;
};

inline Store& Titles()
{
    return Store::Instance();
}
} // namespace GameLogic::Titles
