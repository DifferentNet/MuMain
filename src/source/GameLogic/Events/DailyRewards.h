#pragma once

#include <chrono>
#include <string>
#include <vector>

// The daily login rewards for the daily rewards window (key N).
//
// Our server's add-on (MU Moldova, DailyLoginRewards) gives an account the reward of the next day of a cycle once
// it has played long enough in a UTC day; a missed day pauses the streak. The window asks with "/dailyinfo"; the
// answer comes as blue server messages, which this takes out of the chat: "#DLY0|streak day|counted today (0/1)|
// seconds today|seconds a day needs|waiting items|days in the cycle", then "#DLY|day|reward" per day of the
// cycle, then "#DLY.|". "/daily" hands out the rewards that waited for room in the inventory.
namespace GameLogic::DailyRewards
{
struct Day
{
    int Number = 0;
    // What the day gives, as the server words it: "Jewel of Chaos x2, Jewel of Life".
    std::wstring Reward;
};

struct Status
{
    // The last day of the cycle that counted, 0 before the first.
    int StreakDay = 0;
    // Whether today (UTC) already counted.
    bool CountedToday = false;
    // The account's online time today when the answer came, and what a day needs.
    int SecondsToday = 0;
    int SecondsNeeded = 0;
    // Reward items that wait for room in the inventory.
    int Waiting = 0;
    int DaysInCycle = 0;
    // When the answer came, to count the time on until the next one.
    std::chrono::steady_clock::time_point ReceivedAt{};
    // The same on the wall clock, to notice a new UTC day.
    std::chrono::system_clock::time_point ReceivedWall{};
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the daily reward lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the state; at most once every few seconds.
    void Request();

    // Asks the server to hand out the rewards that wait ("/daily"; its answer goes to the chat).
    void TakeWaiting();

    bool HasInfo() const { return m_hasInfo; }
    std::chrono::steady_clock::time_point LastRequest() const { return m_lastRequest; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Day>& Days() const { return m_days; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    Status m_status;
    std::vector<Day> m_days;
    Status m_incomingStatus;
    std::vector<Day> m_incomingDays;
    bool m_receiving = false;
    bool m_hasInfo = false;
    std::chrono::steady_clock::time_point m_lastRequest{};
};

inline Store& Rewards()
{
    return Store::Instance();
}
} // namespace GameLogic::DailyRewards
