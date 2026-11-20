#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>

// The hero's Combat Power (CP) as the server's /cp counts it, for the windows that show it outside the advanced stats
// tab: the inventory (in place of the socket option, which this server doesn't have) and the character window's
// summary, under the level (ghinzuro 2026-11-20, «A и B вместе»). The client can't count it alone (the stats part is
// the server's), so it is the advanced stats answer's "cp" (AdvancedStats.h).
namespace GameLogic::AdvancedStats
{
// The last answer's CP, nullopt before the server answered for this character.
std::optional<long long> KnownHeroCombatPower();

// The CP with a comma every three digits: "12,345".
std::wstring FormatCombatPower(long long value);

// Asks the server for the CP while a window that shows it is open: when it opens, when the character changed, and
// when the hero's equipment or stats change. A window keeps one and calls Update() every frame, shown or not.
class HeroCombatPowerWatch
{
public:
    void Update(bool shown);

private:
    // What the CP follows on the client's side: the equipment's CP, the level and resets, the stats with their
    // bonuses, and whether an item is on the cursor (a worn item taken off leaves its slot at once, but the server
    // counts it off only when it is put down, so putting it down asks again).
    using Inputs = std::array<long long, 14>;
    static Inputs ReadInputs();

    bool m_shown = false;
    // A request is owed since this time; none while it is unset.
    std::optional<std::chrono::steady_clock::time_point> m_owedSince;
    unsigned m_version = 0;
    std::optional<Inputs> m_inputs;
    std::chrono::steady_clock::time_point m_nextInputCheck{};
};
} // namespace GameLogic::AdvancedStats
