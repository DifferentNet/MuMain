#pragma once

#include <chrono>
#include <string>
#include <vector>

// The Tier Maps for their window (opened at Jerint the Assistant in Devias).
//
// Our server's add-on (MU Moldova, TierMaps) runs the endgame dungeons: six tiers in the Imperial Guardian fortress,
// a party goes in from the NPC with a Gaion's Order, three runs a day per character, the party's Combat Power
// deciding the tiers. Talking to the NPC sends "#TMO|" (open the window) and the state; "/tierinfo" asks for the
// state again. The state comes as blue server messages, which this takes out of the chat:
//   "#TM0|near|leader|party size|ticket|runs a day|seconds to the reset|cp enforced|average cp|lowest cp|member share %|day|layout|boss|in a run"
//   "#TMT|tier|name|cp|open|record seconds|record members|fixed day|layout|boss" per tier
//   "#TMP|name|cp|runs left|near" per party member, the leader first
//   "#TM.|"
// "/tierenter <tier>" takes the party in; its answer is "#TME|ok|text".
namespace GameLogic::TierMaps
{
struct Tier
{
    int Number = 0;
    std::wstring Name;
    int CombatPower = 0;
    // The party's Combat Power lets it in.
    bool Open = false;
    // The fastest win: seconds (0 = none) and who.
    int RecordSeconds = 0;
    std::wstring RecordMembers;
    // The fortress layout it runs on now, and its last boss.
    std::wstring Layout;
    std::wstring Boss;
};

struct Member
{
    std::wstring Name;
    int CombatPower = 0;
    int RunsLeft = 0;
    // Stands at the NPC.
    bool Near = false;
};

struct Status
{
    // This character stands at the NPC, and leads its party (or is alone).
    bool Near = false;
    bool Leader = false;
    int PartySize = 1;
    // The leader has a Gaion's Order.
    bool Ticket = false;
    int RunsPerDay = 3;
    // Seconds to the next UTC day when the answer came.
    int SecondsToReset = 0;
    bool Enforced = true;
    int AverageCombatPower = 0;
    int LowestCombatPower = 0;
    int MemberSharePercent = 70;
    std::wstring Layout;
    std::wstring Boss;
    bool InRun = false;
    std::chrono::steady_clock::time_point ReceivedAt{};
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the Tier Maps lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // True once after the NPC asked the client to open the window.
    bool TakeOpenRequest();

    // Asks the server for the state; at most once every few seconds.
    void Request();

    // The party leader takes the party into a tier.
    void Enter(int tier);

    bool HasInfo() const { return m_hasInfo; }
    std::chrono::steady_clock::time_point LastRequest() const { return m_lastRequest; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Tier>& Tiers() const { return m_tiers; }
    const std::vector<Member>& Members() const { return m_members; }

    // The answer to the last Enter, and whether one is still awaited.
    bool IsEntering() const { return m_entering; }
    bool LastEnterSucceeded() const { return m_lastEnterOk; }
    const std::wstring& LastEnterText() const { return m_lastEnterText; }
    // Counts up on every answer to Enter, so the window notices a new one.
    int EnterAnswers() const { return m_enterAnswers; }

    // Changes whenever new data came, so the window rebuilds only then.
    int Version() const { return m_version; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    Status m_status;
    std::vector<Tier> m_tiers;
    std::vector<Member> m_members;
    Status m_incomingStatus;
    std::vector<Tier> m_incomingTiers;
    std::vector<Member> m_incomingMembers;
    bool m_receiving = false;
    bool m_hasInfo = false;
    bool m_openRequested = false;
    bool m_entering = false;
    bool m_lastEnterOk = false;
    std::wstring m_lastEnterText;
    int m_enterAnswers = 0;
    int m_version = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
    std::chrono::steady_clock::time_point m_enterSentAt{};
};

inline Store& Maps()
{
    return Store::Instance();
}
} // namespace GameLogic::TierMaps
