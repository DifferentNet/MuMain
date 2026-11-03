#pragma once

#include <chrono>
#include <string>
#include <vector>

// The Golden Archer's Rena shop, for its window (opened by the Golden Archer, NPC 236).
//
// Our server's add-on (MU Moldova, RenaShop) takes Rena (a stackable Rena in the bag is handed in for balance) and
// lets the balance be spent on shop lines and Golden Arrow spins. Talking to the NPC sends "#RNO|" (open the window)
// and the data; "/renainfo" asks for the data again. The data comes as blue server messages, which this takes out of
// the chat:
//   "#RN0|balance|Rena in the bag|handed in by this character|handed this week|week place (0 none)|spin price|near 0/1|waiting items"
//   "#RNS|key|name|price|zen|group|number|level|count" per shop line (group -1 when it is zen only)
//   "#RNL|key|name|chance %|announce 0/1|group|number|level|count|zen min|zen max" per lottery prize (the last is the zen rest prize)
//   "#RNM|count|title id|English|Russian|reached 0/1" per title milestone
//   "#RNW|place|name|handed" per place of the week's top 5
//   "#RN.|"
// The client's actions go out as chat commands: "/renagive", "/renabuy key [count]", "/renaspin [count]" (1..10).
// Their answer is "#RNK|ok|text"; each spin result comes first as "#RNR|prize key|text", then the data again.
namespace GameLogic::RenaShop
{
struct Status
{
    long long Balance = 0;
    long long InBag = 0;
    long long HandedCharacter = 0;
    long long HandedWeek = 0;
    int WeekPlace = 0;
    long long SpinPrice = 0;
    // Stands at the Golden Archer.
    bool Near = false;
    long long Waiting = 0;
};

struct Line
{
    std::wstring Key;
    std::wstring Name;
    long long Price = 0;
    long long Zen = 0;
    // The item (group -1: zen only).
    int Group = -1;
    int Number = 0;
    int Level = 0;
    int Count = 1;
};

struct Prize
{
    std::wstring Key;
    std::wstring Name;
    // As the server wrote it ("5", "0.5").
    std::wstring Chance;
    bool Announce = false;
    int Group = -1;
    int Number = 0;
    int Level = 0;
    int Count = 1;
    long long ZenMin = 0;
    long long ZenMax = 0;
};

struct Milestone
{
    long long Count = 0;
    int TitleId = 0;
    std::wstring English;
    std::wstring Russian;
    bool Reached = false;
};

struct WeekEntry
{
    int Place = 0;
    std::wstring Name;
    long long Handed = 0;
};

struct SpinResult
{
    std::wstring Key;
    std::wstring Text;
};

class Store
{
public:
    // The most spins / pieces one action asks for (the server's limits).
    static constexpr int MaximumSpins = 10;
    static constexpr int MaximumBuyCount = 100;

    static Store& Instance();

    // Takes a server message. True when it was one of the shop's lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // True once after the NPC asked the client to open the window.
    bool TakeOpenRequest();

    // Asks the server for the data; at most once every few seconds.
    void Request();

    // The actions; nothing while one is awaited or when the numbers are out of range.
    void Give();
    void Buy(const std::wstring& key, int count);
    void Spin(int count);

    // How many of a shop line / spins the balance pays for.
    int MostBuyable(const Line& line) const;
    int MostSpins() const;

    bool HasInfo() const { return m_hasInfo; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Line>& Lines() const { return m_lines; }
    const std::vector<Prize>& Prizes() const { return m_prizes; }
    const std::vector<Milestone>& Milestones() const { return m_milestones; }
    const std::vector<WeekEntry>& Week() const { return m_week; }
    // The results of the last spin(s) (up to 10).
    const std::vector<SpinResult>& Results() const { return m_results; }

    // The answer to the last action, and whether one is still awaited.
    bool IsBusy() const { return m_busy; }
    bool LastAnswerSucceeded() const { return m_lastOk; }
    const std::wstring& LastAnswerText() const { return m_lastText; }
    // Counts up on every answer, so the window notices a new one.
    int Answers() const { return m_answers; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    void ClearIncoming();
    void TakeHeader(const std::vector<std::wstring>& fields);
    void Send(const std::wstring& command);
    void ExpireBusy();

    Status m_status;
    std::vector<Line> m_lines;
    std::vector<Prize> m_prizes;
    std::vector<Milestone> m_milestones;
    std::vector<WeekEntry> m_week;
    Status m_incomingStatus;
    std::vector<Line> m_incomingLines;
    std::vector<Prize> m_incomingPrizes;
    std::vector<Milestone> m_incomingMilestones;
    std::vector<WeekEntry> m_incomingWeek;
    std::vector<SpinResult> m_results;
    bool m_receiving = false;
    bool m_hasInfo = false;
    bool m_openRequested = false;
    bool m_busy = false;
    // The next spin result starts a new list.
    bool m_newSpin = true;
    bool m_lastOk = false;
    std::wstring m_lastText;
    int m_answers = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
    std::chrono::steady_clock::time_point m_busySentAt{};
};

inline Store& Rena()
{
    return Store::Instance();
}
} // namespace GameLogic::RenaShop
