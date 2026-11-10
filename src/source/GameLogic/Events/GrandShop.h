#pragma once

#include <chrono>
#include <string>
#include <vector>

// The Grand Reset shop of ancient sets, for its window (opened by the chat command "/grshop").
//
// Our server's add-on (MU Moldova, GrandShop) sells ancient set pieces and the Jewel of Ancient for Grand Marks, a
// currency earned by every Grand Reset. The shop's answer comes as blue server messages, which this takes out of the
// chat:
//   "#GSO|marks|grand resets|safe zone 0/1"
//   "#GS|key|kind|group|number|set type|price|set name" per line (kind 0 a classic set piece, 1 a high-tier set
//       piece, 2 the Jewel of Ancient; set type 1/2, 0 for the jewel; key "group.number.setType", "jewel" for the jewel)
//   "#GS.|"
//   "#GS!|text" when the shop cannot open (no add-on table, in an event)
// A purchase is the chat command "/grbuy key"; its answer is "#GSK|1 or 0|marks|text".
namespace GameLogic::GrandShop
{
enum class Kind
{
    Classic = 0,
    HighTier = 1,
    Jewel = 2,
};

struct Status
{
    long long Marks = 0;
    int GrandResets = 0;
    // Buying works only in a safe zone.
    bool SafeZone = false;
};

struct Line
{
    std::wstring Key;
    Kind LineKind = Kind::Classic;
    int Group = 0;
    int Number = 0;
    // 1 or 2; 0 for the jewel.
    int SetType = 0;
    long long Price = 0;
    // The set's name as the server wrote it (the item's own name comes from the client's set table).
    std::wstring SetName;
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the shop's lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // True once after the server asked the client to open the window.
    bool TakeOpenRequest();

    // The purchase of the line with the key; nothing while an answer is awaited or when the key is empty.
    void Buy(const std::wstring& key);

    bool HasInfo() const { return m_hasInfo; }
    // Why the shop could not open ("#GS!|"); empty when it could.
    const std::wstring& FailText() const { return m_failText; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Line>& Lines() const { return m_lines; }

    // The answer to the last purchase, and whether one is still awaited.
    bool IsBusy() const;
    bool LastAnswerSucceeded() const { return m_lastOk; }
    const std::wstring& LastAnswerText() const { return m_lastText; }
    // Counts up on every answer, so the window notices a new one.
    int Answers() const { return m_answers; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    void TakeHeader(const std::vector<std::wstring>& fields);
    void TakeLine(const std::vector<std::wstring>& fields);
    void TakeAnswer(const std::vector<std::wstring>& fields);

    Status m_status;
    std::vector<Line> m_lines;
    Status m_incomingStatus;
    std::vector<Line> m_incomingLines;
    bool m_receiving = false;
    bool m_hasInfo = false;
    bool m_openRequested = false;
    std::wstring m_failText;
    bool m_busy = false;
    bool m_lastOk = false;
    std::wstring m_lastText;
    int m_answers = 0;
    std::chrono::steady_clock::time_point m_busySentAt{};
};

inline Store& Grand()
{
    return Store::Instance();
}
} // namespace GameLogic::GrandShop
