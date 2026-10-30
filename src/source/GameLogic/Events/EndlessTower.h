#pragma once

#include <chrono>
#include <string>
#include <vector>

// The Endless Tower for its window (opened at Lugard in Lorencia and Devias).
//
// Our server's add-on (MU Moldova, EndlessTower) runs the tower: a party of 3 to 5 climbs waves of monsters, a boss
// every 10th wave, for 30 minutes or until everyone inside died; each member pays the entry in zen and spends one of
// the day's runs. Talking to the NPC sends "#ETO|" (open the window) and the state; "/towerinfo" asks for the state
// again. The state comes as blue server messages, which this takes out of the chat:
//   "#ET0|near|leader|party size|minimum party|maximum party|price|runs a day|seconds to the reset|in a run"
//   "#ETP|name|runs left|near|has the zen|best height" per party member, the leader first
//   "#ETR|place|name|class|height|seconds|party size" per place of the public ranking's top
//   "#ETB|from wave|text" per boss reward, "#ETW|from wave|text" per reward by the wave reached
//   "#ET.|"
// "/towerenter" takes the party in; its answer is "#ETE|ok|text".
namespace GameLogic::EndlessTower
{
struct Member
{
    std::wstring Name;
    int RunsLeft = 0;
    // Stands at the NPC, on the leader's map.
    bool Near = false;
    // Carries the entry price.
    bool HasZen = false;
    // The highest wave this character cleared (0 = none yet).
    int BestHeight = 0;
};

struct TopEntry
{
    int Place = 0;
    std::wstring Name;
    std::wstring ClassName;
    int Height = 0;
    int Seconds = 0;
    int PartySize = 0;
};

struct Reward
{
    int FromWave = 0;
    std::wstring Text;
};

struct Status
{
    // This character stands at the NPC, and leads its party (or is alone).
    bool Near = false;
    bool Leader = false;
    int PartySize = 1;
    int MinimumParty = 3;
    int MaximumParty = 5;
    long long Price = 0;
    int RunsPerDay = 2;
    // Seconds to the next UTC day when the answer came.
    int SecondsToReset = 0;
    bool InRun = false;
    std::chrono::steady_clock::time_point ReceivedAt{};
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the tower's lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // True once after the NPC asked the client to open the window.
    bool TakeOpenRequest();

    // Asks the server for the state; at most once every few seconds.
    void Request();

    // The party leader takes the party in.
    void Enter();

    bool HasInfo() const { return m_hasInfo; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Member>& Members() const { return m_members; }
    const std::vector<TopEntry>& Top() const { return m_top; }
    const std::vector<Reward>& BossRewards() const { return m_bossRewards; }
    const std::vector<Reward>& HeightRewards() const { return m_heightRewards; }

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
    void ClearIncoming();

    Status m_status;
    std::vector<Member> m_members;
    std::vector<TopEntry> m_top;
    std::vector<Reward> m_bossRewards;
    std::vector<Reward> m_heightRewards;
    Status m_incomingStatus;
    std::vector<Member> m_incomingMembers;
    std::vector<TopEntry> m_incomingTop;
    std::vector<Reward> m_incomingBossRewards;
    std::vector<Reward> m_incomingHeightRewards;
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

inline Store& Tower()
{
    return Store::Instance();
}
} // namespace GameLogic::EndlessTower
