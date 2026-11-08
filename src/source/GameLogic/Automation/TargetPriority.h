// MU Helper: a boss or a much stronger monster first (ghinzuro, 2026-11-08: «выбирал цель по максимальному хп и
// урону вокруг себя», a boss in Kalima, the character kept running to the weak monsters around it).
//
// The client knows only a monster's health in percent, so the server's add-on tells the maximum health and the damage
// of the monsters around the character (server addons/src/HelperTargets.cs, hidden "/mst"):
//     #MST0|id:health:damage|id:health:damage|...   a new list (also "#MST0|" alone, no monsters)
//     #MST|id:health:damage|...                     more of the same list
// The strength of a monster is health x damage. One that is StrongFactor times the weakest one around is "strong": the
// helper goes for the strongest of those and keeps it until it dies, unless one StrongFactor times stronger still
// turns up. Among monsters of about the same strength nothing changes: the nearest, as before.
#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GameLogic::Automation
{
inline constexpr std::wstring_view StrengthListStart = L"#MST0|";
inline constexpr std::wstring_view StrengthListMore = L"#MST|";

// How many times the weakest one around a monster has to be to count as strong (health x damage: a monster with
// twice the health and twice the damage is 4 times as strong).
inline constexpr double StrongFactor = 4.0;

struct MonsterStrength
{
    long long health = 0;
    long long damage = 0;
};

[[nodiscard]] inline double StrengthScore(const MonsterStrength& strength)
{
    if (strength.health <= 0 || strength.damage <= 0)
    {
        return 0.0; // a gate or a statue that never hits back is no boss
    }
    return static_cast<double>(strength.health) * static_cast<double>(strength.damage);
}

struct StrengthLine
{
    bool startsList = false;
    std::vector<std::pair<int, MonsterStrength>> entries; // client key (object id & 0x7FFF), strength
};

namespace Detail
{
[[nodiscard]] inline bool ParseWhole(std::wstring_view text, long long& value)
{
    if (text.empty() || text.size() > 18)
    {
        return false;
    }
    long long result = 0;
    for (const wchar_t c : text)
    {
        if (c < L'0' || c > L'9')
        {
            return false;
        }
        result = result * 10 + (c - L'0');
    }
    value = result;
    return true;
}
} // namespace Detail

// A line of the server's answer, or nothing when it isn't one. Entries that don't read are skipped.
[[nodiscard]] inline std::optional<StrengthLine> ParseStrengthLine(std::wstring_view text)
{
    StrengthLine line;
    if (text.starts_with(StrengthListStart))
    {
        line.startsList = true;
        text.remove_prefix(StrengthListStart.size());
    }
    else if (text.starts_with(StrengthListMore))
    {
        text.remove_prefix(StrengthListMore.size());
    }
    else
    {
        return std::nullopt;
    }

    while (!text.empty())
    {
        const size_t bar = text.find(L'|');
        const std::wstring_view entry = text.substr(0, bar);
        text = bar == std::wstring_view::npos ? std::wstring_view{} : text.substr(bar + 1);

        const size_t first = entry.find(L':');
        const size_t second = first == std::wstring_view::npos ? std::wstring_view::npos : entry.find(L':', first + 1);
        if (second == std::wstring_view::npos)
        {
            continue;
        }
        long long id = 0;
        MonsterStrength strength;
        if (!Detail::ParseWhole(entry.substr(0, first), id)
            || !Detail::ParseWhole(entry.substr(first + 1, second - first - 1), strength.health)
            || !Detail::ParseWhole(entry.substr(second + 1), strength.damage))
        {
            continue;
        }
        line.entries.emplace_back(static_cast<int>(id & 0x7FFF), strength);
    }
    return line;
}

// A monster the helper may attack now, with its strength (0 = not known) and its distance in tiles.
struct TargetCandidate
{
    int key = -1;
    double score = 0.0;
    int distance = 0;
};

// The strong monster to go for, or -1 when there is none and the helper picks as before. `currentKey` is the
// current target (-1 = none): a strong one is kept while no monster `factor` times stronger is around.
[[nodiscard]] inline int PickStrongTarget(const std::vector<TargetCandidate>& candidates, int currentKey,
                                          double factor = StrongFactor)
{
    double weakest = 0.0;
    int known = 0;
    for (const TargetCandidate& c : candidates)
    {
        if (c.score <= 0.0)
        {
            continue;
        }
        weakest = known == 0 ? c.score : std::min(weakest, c.score);
        ++known;
    }
    if (known < 2)
    {
        return -1; // nothing to compare with
    }

    const TargetCandidate* strongest = nullptr;
    const TargetCandidate* current = nullptr;
    for (const TargetCandidate& c : candidates)
    {
        if (c.score < weakest * factor)
        {
            continue;
        }
        if (c.key == currentKey)
        {
            current = &c;
        }
        if (strongest == nullptr || c.score > strongest->score
            || (c.score == strongest->score && c.distance < strongest->distance))
        {
            strongest = &c;
        }
    }
    if (strongest == nullptr)
    {
        return -1;
    }
    if (current != nullptr && strongest->score < current->score * factor)
    {
        return current->key; // keep the strong target it has
    }
    return strongest->key;
}

// The strengths the server last told, by client key. Fed by the hidden lines (TargetPriority.cpp).
class MonsterStrengths
{
public:
    // Takes one line of the answer; false when it isn't one.
    bool Take(std::wstring_view text, std::chrono::steady_clock::time_point now)
    {
        auto line = ParseStrengthLine(text);
        if (!line)
        {
            return false;
        }
        if (line->startsList)
        {
            m_strengths.clear();
        }
        for (const auto& [key, strength] : line->entries)
        {
            m_strengths[key] = strength;
        }
        m_lastAnswer = now;
        return true;
    }

    // 0 when the server told nothing about this monster.
    [[nodiscard]] double ScoreOf(int key) const
    {
        const auto found = m_strengths.find(key);
        return found == m_strengths.end() ? 0.0 : StrengthScore(found->second);
    }

    [[nodiscard]] std::chrono::steady_clock::time_point LastAnswer() const { return m_lastAnswer; }

    void Clear()
    {
        m_strengths.clear();
        m_lastAnswer = {};
    }

private:
    std::unordered_map<int, MonsterStrength> m_strengths;
    std::chrono::steady_clock::time_point m_lastAnswer{};
};

// The client's one list, filled from the server's lines, and its request.
MonsterStrengths& Strengths();

// Asks the server for the strengths around the character, at most every RequestInterval.
void RequestStrengths();

inline constexpr auto StrengthRequestInterval = std::chrono::milliseconds(1500);
// A list older than this is not used: a server without the add-on never answers, and a monster's key is reused.
inline constexpr auto StrengthMaxAge = std::chrono::seconds(5);
} // namespace GameLogic::Automation
