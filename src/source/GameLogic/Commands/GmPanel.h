#pragma once

#include <chrono>
#include <string>
#include <vector>

// The game master panel's data (the GM window, key Y).
//
// Our server's add-on (MU Moldova, GmPanel) answers one hidden chat command, "/gm <request>", for characters of
// game master accounts (Account State GameMaster); the rights are checked there on every request, by the account
// and the role in the add-on's settings (admin or moderator). Its answers come as blue server messages, which this
// takes out of the chat:
//   "#GM0|role|character|hidden 0/1|gm mode 0/1"   for "/gm hello" (role admin, mod, or empty: not a GM)
//   "#GMP0|n", "#GMP|name|account|class|level|resets|map|x|y|cp|flags", "#GMP.|"   players in game
//      (flags: g GM, m muted, h hidden, e in an event, t in a Tier Map run; the account only for admins)
//   "#GMF0|n", "#GMF|name|account|class|online 0/1|state|muted until|banned until", "#GMF.|"   a name search
//   "#GMB0|n", "#GMB|name|account|ban/tban/mute|until|gm|reason", "#GMB.|"   bans and mutes
//   "#GMW0|n", "#GMW|warp index|name|map|x|y", "#GMW.|"   the teleport places
//   "#GML0|n", "#GML|utc time|gm|op|target|detail", "#GML.|"   the last actions, the newest first
//   "#GMR|ok/err|op|text"   the answer to an action (kick, ban, warp, ...)
// Times are UTC, "yyyy-MM-dd HH:mm".
namespace GameLogic::GmPanel
{
// The most names a search answers with (the add-on's GmPanelStore.FindAsync).
inline constexpr size_t FindLimit = 30;

enum class Role
{
    // Not asked yet (or the character changed).
    Unknown,
    None,
    Moderator,
    Admin,
};

struct OnlinePlayer
{
    std::wstring Name;
    // Empty for moderators.
    std::wstring Account;
    std::wstring ClassName;
    int Level = 0;
    int Resets = 0;
    int Map = -1;
    int X = 0;
    int Y = 0;
    int CombatPower = 0;
    std::wstring Flags;

    bool Has(wchar_t flag) const { return Flags.find(flag) != std::wstring::npos; }
};

struct FoundCharacter
{
    std::wstring Name;
    std::wstring Account;
    std::wstring ClassName;
    bool Online = false;
    // normal, spectator, gm, ban (forever), tban (timed).
    std::wstring State;
    // Empty when not muted / not banned; "forever" for a ban without an end.
    std::wstring MutedUntil;
    std::wstring BannedUntil;
};

struct Penalty
{
    std::wstring Name;
    std::wstring Account;
    // ban (forever), tban (timed), mute.
    std::wstring Kind;
    std::wstring Until;
    std::wstring GmName;
    std::wstring Reason;
};

struct Warp
{
    int Index = 0;
    std::wstring Name;
    int Map = 0;
    int X = 0;
    int Y = 0;
};

struct LogEntry
{
    std::wstring Time;
    std::wstring GmName;
    std::wstring Op;
    std::wstring Target;
    std::wstring Detail;
};

// One of the lists the server sends: the rows of its last complete answer.
template <typename Row> struct List
{
    std::vector<Row> Rows;
    // An answer came since the character joined.
    bool Received = false;
    // Counts up with every complete answer, so a window rebuilds only then.
    int Version = 0;

    std::vector<Row> Incoming;
    bool Receiving = false;
};

struct Result
{
    bool Ok = false;
    std::wstring Op;
    std::wstring Text;
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was one of the GM panel's lines, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server whether this character may use the panel ("/gm hello"); at most once every few seconds.
    // With openWhenGm, TakeOpenRequest() says so once the answer says yes.
    void Hello(bool openWhenGm);
    // True once after a Hello(true) whose answer gave a role.
    bool TakeOpenRequest();

    // Sends "/gm <request>", e.g. L"players" or L"kick Name reason".
    void Send(const std::wstring& request);

    Role GetRole() const { return m_role; }
    bool IsGm() const { return m_role == Role::Admin || m_role == Role::Moderator; }
    bool IsAdmin() const { return m_role == Role::Admin; }
    bool Hidden() const { return m_hidden; }
    bool GmMode() const { return m_gmMode; }
    // Counts up with every answer to Hello.
    int HelloVersion() const { return m_helloVersion; }

    const List<OnlinePlayer>& Players() const { return m_players; }
    const List<FoundCharacter>& Found() const { return m_found; }
    const List<Penalty>& Penalties() const { return m_penalties; }
    const List<Warp>& Warps() const { return m_warps; }
    const List<LogEntry>& Log() const { return m_log; }

    // The answer to the last action, and a count of them, so a window notices a new one.
    const Result& LastResult() const { return m_result; }
    int ResultVersion() const { return m_resultVersion; }

    // Forgets everything, e.g. when the character changed.
    void Reset();

private:
    Role m_role = Role::Unknown;
    bool m_hidden = false;
    bool m_gmMode = false;
    int m_helloVersion = 0;
    bool m_openWhenGm = false;
    bool m_openRequested = false;
    std::chrono::steady_clock::time_point m_lastHello{};

    List<OnlinePlayer> m_players;
    List<FoundCharacter> m_found;
    List<Penalty> m_penalties;
    List<Warp> m_warps;
    List<LogEntry> m_log;

    Result m_result;
    int m_resultVersion = 0;
};

inline Store& Panel()
{
    return Store::Instance();
}
} // namespace GameLogic::GmPanel
