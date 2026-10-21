#pragma once

#include <chrono>
#include <string>
#include <vector>

// The server's event times for the event timer window (key O).
//
// The window asks with "/eventtimes", a chat command of our server's add-on (MU Moldova). The answer comes as
// blue server messages, which this takes out of the chat so they don't push the rest of the log away:
// "#EVT0|count", then per event "#EVT|phase|seconds|weekly|name", then "#EVT.|" (EventTimesChatCommandPlugIn).
namespace GameLogic::EventTimes
{
enum class Phase
{
    Next,         // starts later
    Running,      // runs now (an invasion, Happy Hour, Crywolf's battle)
    EntranceOpen, // a mini game's entrance is open
};

struct Entry
{
    std::wstring Name;
    Phase State = Phase::Next;
    // An event that isn't daily, whose day is worth showing.
    bool Weekly = false;
    // When it starts (Next) or ends (Running, EntranceOpen), on the steady clock and the wall clock.
    std::chrono::steady_clock::time_point Deadline;
    std::chrono::system_clock::time_point WallDeadline;
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the event time lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the times; at most once every few seconds.
    void Request();

    // Whether an answer came since the last Request(), and whether any did at all.
    bool IsAnswered() const { return m_answered; }
    bool HasTimes() const { return m_hasTimes; }
    std::chrono::steady_clock::time_point LastRequest() const { return m_lastRequest; }

    const std::vector<Entry>& Entries() const { return m_entries; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    std::vector<Entry> m_entries;
    std::vector<Entry> m_incoming;
    bool m_receiving = false;
    bool m_answered = false;
    bool m_hasTimes = false;
    std::chrono::steady_clock::time_point m_lastRequest{};
};

inline Store& Times()
{
    return Store::Instance();
}
} // namespace GameLogic::EventTimes
