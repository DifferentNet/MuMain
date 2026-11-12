#pragma once

#include <chrono>
#include <string>
#include <string_view>

// The ping label under the MU Helper bar: the round trip to the game server, as our server's add-on measures it
// (MU Moldova, NetworkQuality.cs: a C1 03 71 every 10 s, which MuMain answers at once).
//
// On entering the world the client sends the hidden "/pinghud"; from then on the server sends one blue message per
// answered ping, which this takes out of the chat: "#PNG|last|median|kind|network", whole milliseconds, kind "d"
// (desktop) or "b" (browser), "-" for a number not known yet. A server without the add-on never answers, and the
// label stays hidden.
namespace GameLogic::PingHud
{
struct Reading
{
    int lastMs = -1;     // the last round trip, -1 before the first answer
    int medianMs = -1;   // the median of the last five minutes
    int networkMs = -1;  // the network's own round trip (the kernel's, or the browser bridge's), -1 if unknown
    bool browser = false;
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was a ping line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the lines ("/pinghud").
    void Request();

    // Forgets the last character's numbers.
    void Reset();

    bool HasReading() const { return m_reading.lastMs >= 0; }
    const Reading& Current() const { return m_reading; }
    // True when no line came for longer than three pings: the server waits for an answer that is late or lost.
    bool IsStale() const;

    // Parses "#PNG|…"; false for another line or a broken one.
    static bool Parse(std::wstring_view text, Reading& reading);

private:
    Reading m_reading;
    std::chrono::steady_clock::time_point m_received{};
};

inline Store& Ping()
{
    return Store::Instance();
}
} // namespace GameLogic::PingHud
