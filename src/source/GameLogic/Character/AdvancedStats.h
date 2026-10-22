#pragma once

#include <chrono>
#include <map>
#include <string>
#include <string_view>

// The character's advanced stats for the advanced stats tab of the character window: chances of critical,
// excellent and double damage, ignoring defense and SD, reflect, damage decrease and the like.
//
// The client doesn't know these totals (buffs, wings and the server's own formulas are in them), so the tab asks
// our server's add-on (MU Moldova, AdvancedStatsChatCommandPlugIn) with "/advstats" while it is open. The answer
// is one blue server message, which this takes out of the chat: "#ADV|key=value|...", values as the tab shows
// them (percent or damage points). Keys this client doesn't know are kept and ignored.
namespace GameLogic::AdvancedStats
{
class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was the advanced stats line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the stats; at most once every two seconds.
    void Request();

    bool HasInfo() const { return m_hasInfo; }
    // The value of a key, or nullptr before the first answer or for a key the server didn't send.
    const double* Find(std::wstring_view key) const;
    // Counts the answers, so a window rebuilds its lines only when a new one came.
    unsigned Version() const { return m_version; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    std::map<std::wstring, double, std::less<>> m_values;
    bool m_hasInfo = false;
    unsigned m_version = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
};

inline Store& Stats()
{
    return Store::Instance();
}
} // namespace GameLogic::AdvancedStats
