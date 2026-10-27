#pragma once

#include <chrono>
#include <string>
#include <vector>

// The battle pass for its window (key X).
//
// Our server's add-on (MU Moldova, BattlePass) keeps a season's track of levels with their rewards and the quests
// that give points. The window asks with "/bpinfo"; the answer comes as blue server messages, which this takes out
// of the chat: "#BP0|..." (the season and the character's progress), "#BPL|..." per level of the track, "#BPQ|..."
// per quest (daily, then weekly by week, then seasonal), then "#BP.|". "#BP!|" asks a client that has the window
// to ask again (something it shows changed); "#BP!|off" says the pass is off. The buttons send "/bpclaim [level]",
// "/bptake" and "/bpreroll slot". See the protocol note of stage 1 (bp-protocol.md).
namespace GameLogic::BattlePass
{
enum class ItemKind
{
    // An item of the game: Group, Number, Level, Count.
    Item,
    // A random excellent item for the character's class: Options, Luck, Level.
    Excellent,
    // A sign of the season (title, aura, trophy, fenrir): Mark, Season.
    Mark,
    // An item that isn't in the game yet (Jewel of Fate before its stage); it waits for the server.
    Placeholder,
};

struct RewardItem
{
    ItemKind Kind = ItemKind::Item;
    int Group = 0;
    int Number = 0;
    int Level = 0;
    int Count = 1;
    // Excellent: how many options and whether with luck.
    int Options = 0;
    bool Luck = false;
    // Mark: its id and season.
    std::wstring Mark;
    int Season = 0;
};

enum class LevelState
{
    Locked = 0,
    Claimable = 1,
    Claimed = 2,
};

struct Level
{
    int Number = 0;
    LevelState State = LevelState::Locked;
    std::vector<RewardItem> Items;
    // The reward as the server words it ("Jewel of Bless x5, Box of Kundun +2"); empty when it has none.
    std::wstring Text;
};

enum class QuestKind
{
    Daily,
    Weekly,
    Seasonal,
};

struct Quest
{
    QuestKind Kind = QuestKind::Daily;
    // The week of the season a weekly quest belongs to; 0 for the others.
    int Week = 0;
    // The slot: 1-3 for a daily quest (what /bpreroll takes), 1-6 in its week, 1-14 for a seasonal one.
    int Slot = 0;
    std::wstring Id;
    bool Done = false;
    int Progress = 0;
    int Target = 0;
    int Points = 0;
    // The quest as the server words it, the map of the day filled in.
    std::wstring Text;
};

enum class Phase
{
    NotStarted = 0,
    Running = 1,
    Ended = 2,
};

struct Status
{
    int Season = 0;
    std::wstring Name;
    Phase SeasonPhase = Phase::NotStarted;
    // Before the season: seconds until it starts; while it runs: until it ends.
    int SecondsLeft = 0;
    // The level reached on the track (bonus levels apart), and all points of the season.
    int Level = 0;
    int Points = 0;
    // Points inside the current level (or bonus level once the track is done), and what a level needs.
    int LevelPoints = 0;
    int PerLevel = 1000;
    int MaxLevel = 50;
    // Bonus levels past the track: reached, the most a season gives, and the points each needs.
    int Bonus = 0;
    int BonusMax = 0;
    int BonusPer = 2000;
    // Levels and bonus levels reached but not claimed; rewards that wait for room in the inventory.
    int Claimable = 0;
    int Waiting = 0;
    // Whether a daily quest can still be replaced today.
    bool RerollAvailable = false;
    // The season's points multiplier (points already include it), and the week of the season (0 outside it).
    int Multiplier = 1;
    int Week = 0;
    // When the answer came, to count the time on.
    std::chrono::steady_clock::time_point ReceivedAt{};
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the battle pass lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the state. Requests that follow each other closely are put off, not dropped: the window
    // sends the one put off with Pump().
    void Request();
    // Sends a request put off, once the last one is old enough. Called every frame while the window is open.
    void Pump();

    // Claims a level's reward (a bonus level past the track's last), or with 0 all reached levels.
    void Claim(int level = 0);
    // Hands out the rewards that wait for room in the inventory.
    void TakeWaiting();
    // Replaces today's daily quest in a slot (1-3).
    void Reroll(int slot);

    bool HasInfo() const { return m_hasInfo; }
    // The server said the pass is off ("#BP!|off").
    bool IsOff() const { return m_off; }
    // The server said something the window shows changed ("#BP!|") since the last answer.
    bool RefreshWanted() const { return m_refreshWanted; }
    std::chrono::steady_clock::time_point LastRequest() const { return m_lastRequest; }

    const Status& GetStatus() const { return m_status; }
    const std::vector<Level>& Levels() const { return m_levels; }
    const std::vector<Quest>& Quests() const { return m_quests; }
    // Counts up every time a new answer is taken, so a window can tell it apart from the one it showed.
    unsigned int Revision() const { return m_revision; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    Status m_status;
    std::vector<Level> m_levels;
    std::vector<Quest> m_quests;
    Status m_incomingStatus;
    std::vector<Level> m_incomingLevels;
    std::vector<Quest> m_incomingQuests;
    bool m_receiving = false;
    bool m_hasInfo = false;
    bool m_off = false;
    bool m_refreshWanted = false;
    bool m_requestPending = false;
    unsigned int m_revision = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
};

inline Store& Pass()
{
    return Store::Instance();
}
} // namespace GameLogic::BattlePass
