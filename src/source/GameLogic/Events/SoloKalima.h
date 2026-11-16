#pragma once

#include <chrono>
#include <string>
#include <vector>

// Kundun's Lair (solo Kalima) for its window.
//
// Our server's add-on (MU Moldova, SoloKalima) runs a solo dungeon in a copy of a Kalima map: one run a day per
// account, a Mirror of Dimensions to go in, six tiers by the character's Combat Power and maximum HP, up to a few
// curses that make it harder for more loot. Talking to its NPC sends "#SKO|" (open the window); "/soloinfo" asks for
// the state, which comes as blue server messages this takes out of the chat:
//   "#SK0|near|tickets|runs left today|seconds to the reset|cp enforced|my counted cp|my max hp|in a run|max curses"
//   "#SKT|tier|name|cp needed|hp needed|open|kalima n|my best s|server best s|server best name" per tier
//   "#SKC|key|name|description" per curse
//   "#SK.|"
// "/soloenter <tier> [key,key,key]" takes the character in; its answer is "#SKE|ok|text".
namespace GameLogic::SoloKalima
{
struct Tier
{
    int Number = 0;
    std::wstring Name;
    int CombatPower = 0;
    int Health = 0;
    // The character's Combat Power and HP let it in.
    bool Open = false;
    // The Kalima map the tier runs on (2-7).
    int Kalima = 0;
    // The fastest wins in seconds (0 = none): the character's own and the server's, and whose that is.
    int MyBestSeconds = 0;
    int ServerBestSeconds = 0;
    std::wstring ServerBestName;
};

struct Curse
{
    // What "/soloenter" takes; the server's own key.
    std::wstring Key;
    std::wstring Name;
    std::wstring Description;
};

struct Status
{
    // This character stands at the NPC.
    bool Near = false;
    // The Mirrors of Dimensions in the inventory.
    int Tickets = 0;
    int RunsLeft = 0;
    // Seconds to the next UTC day when the answer came.
    int SecondsToReset = 0;
    bool Enforced = true;
    int CombatPower = 0;
    int Health = 0;
    bool InRun = false;
    int MaxCurses = 0;
    std::chrono::steady_clock::time_point ReceivedAt{};
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of Kundun's Lair's lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // True once after the NPC asked the client to open the window.
    bool TakeOpenRequest();

    // Asks the server for the state; at most once every few seconds.
    void Request();

    // Takes the character into a tier with the given curses (their keys).
    void Enter(int tier, const std::vector<std::wstring>& curses);

    bool HasInfo() const { return m_hasInfo; }
    std::chrono::steady_clock::time_point LastRequest() const { return m_lastRequest; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Tier>& Tiers() const { return m_tiers; }
    const std::vector<Curse>& Curses() const { return m_curses; }

    // The answer to the last Enter, and whether one is still awaited.
    bool IsEntering() const { return m_entering; }
    bool LastEnterSucceeded() const { return m_lastEnterOk; }
    // The server's text for it (a refusal's reason); kept until the next Enter or until the window opens again, as the
    // server sends its refresh right after the answer.
    const std::wstring& LastEnterText() const { return m_lastEnterText; }
    // Forgets that text, e.g. when the window opens again.
    void ClearEnterText()
    {
        m_lastEnterText.clear();
    }
    // Counts up on every answer to Enter, so the window notices a new one.
    int EnterAnswers() const { return m_enterAnswers; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    bool TakeHeader(std::wstring_view fields);
    void TakeTier(std::wstring_view fields);
    void TakeCurse(std::wstring_view fields);
    void TakeFooter();
    void TakeEnterAnswer(std::wstring_view fields);

    Status m_status;
    std::vector<Tier> m_tiers;
    std::vector<Curse> m_curses;
    Status m_incomingStatus;
    std::vector<Tier> m_incomingTiers;
    std::vector<Curse> m_incomingCurses;
    bool m_receiving = false;
    bool m_hasInfo = false;
    bool m_openRequested = false;
    bool m_entering = false;
    bool m_lastEnterOk = false;
    std::wstring m_lastEnterText;
    // Set when a refresh starts with an enter text already shown: its footer clears the text.
    int m_enterAnswers = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
    std::chrono::steady_clock::time_point m_enterSentAt{};
};

inline Store& Lair()
{
    return Store::Instance();
}
} // namespace GameLogic::SoloKalima
