#pragma once

#include <optional>
#include <string>
#include <string_view>

// `$hudtest`: local values for the live HUD (UI/HUD/LiveHud.h) in place of the hero's, so every state of the gems, the
// skill slots and the EXP bar can be shown and screenshot (SD loss needs PvP). Only what the driver reads is
// overridden; nothing goes to the server.
//   $hudtest hp|mp|sd|ag <0..1>   a gauge's fill
//   $hudtest expfill <0..1>       the EXP bar's fill, and with it the spark
//   $hudtest cd <seconds>         every skill slot cooling down for that long (at most MaxCooldownSeconds), from now
//   $hudtest exp                  an EXP gain
//   $hudtest lvl                  a level-up (the hero's level shown one higher than the last)
//   $hudtest poison on|off        the hero poisoned or not
//   $hudtest stats                one system line with the live HUD's cost (FormatStats())
//   $hudtest stats on|off         that line also logged at every full window (MU_LOG_INFO: MuError.log, and the
//                                 browser console on the web)
//   $hudtest off                  the hero's own values again, and the stats log off
namespace UI::HUD::LiveHudTest
{
// The longest cooldown `cd` plays; a longer one is cut to it, so its milliseconds always fit an int.
inline constexpr float MaxCooldownSeconds = 3600.f;

struct Overrides
{
    std::optional<float> life;
    std::optional<float> mana;
    std::optional<float> shield;
    std::optional<float> skillMana;
    std::optional<float> expFill;
    std::optional<bool> poisoned;
    std::optional<float> cooldownSeconds;
    // How many cooldowns, EXP gains and level-ups were asked for; the driver plays one per new request.
    int cooldowns = 0;
    int expGains = 0;
    int levelUps = 0;
    // How many stats lines were asked for (the driver answers each new one once), and whether every window is logged.
    int statsRequests = 0;
    bool statsLog = false;
};

// What is left of the cooldown `cd` plays on every skill slot: its share (0..1) and its milliseconds.
struct CooldownLeft
{
    float fraction = 0.f;
    int remainingMs = 0;
};

// Plays `$hudtest cd` back: each new request starts its cooldown afresh when it is first seen.
class CooldownPlayback
{
public:
    // The cooldown left at `now` (seconds) under `overrides`; none while no request runs (none asked for, `cd 0`, or
    // its time is up).
    [[nodiscard]] std::optional<CooldownLeft> At(const Overrides& overrides, double now);

private:
    int m_seen = 0;
    double m_start = 0.0;
};

// The overrides in force this session.
Overrides& Current();

// The arguments after `$hudtest` (e.g. "hp 0.25", "poison on", "off") applied to `overrides`; false, and nothing
// changed, when they are not one of the forms above.
bool Apply(Overrides& overrides, std::wstring_view arguments);

// The reply to arguments Apply() refused.
inline constexpr const wchar_t* Usage =
    L"$hudtest hp|mp|sd|ag|expfill <0..1>, cd <seconds>, exp, lvl, poison on|off, stats [on|off], off";

// The live HUD's cost over one window (UI::HUD::LiveHud::Sync()).
struct SyncStats
{
    double seconds = 0.0;
    int syncs = 0;
    // The syncs past the 1/30 s gate, which rebuild what changed: `rebuilds` meshes in all, at most `maxRebuilds` in
    // one.
    int ticks = 0;
    int rebuilds = 0;
    int maxRebuilds = 0;
    // The syncs' time in microseconds, all of them and the longest.
    long long micros = 0;
    int maxMicros = 0;
};

// Counts the syncs in windows of WindowSeconds: plain sums, nothing allocated.
class StatsWindow
{
public:
    static constexpr double WindowSeconds = 5.0;

    // One sync that ended at `now` (seconds on a steady clock) after `micros`; `ticked` when it got past the gate,
    // rebuilding `rebuilds` meshes. The first one after Reset() only starts the clock.
    void Record(double now, int micros, bool ticked, int rebuilds);
    // The last full window; the running one until the first is full.
    [[nodiscard]] const SyncStats& Latest() const
    {
        return m_hasLast ? m_last : m_running;
    }
    // How many windows have filled since the start; a new count means a new Latest().
    [[nodiscard]] int Filled() const
    {
        return m_filled;
    }
    // Starts over (a new document); Filled() keeps counting.
    void Reset();

private:
    SyncStats m_running;
    SyncStats m_last;
    bool m_hasLast = false;
    double m_start = -1.0;
    int m_filled = 0;
};

// `$hudtest stats`'s line, e.g. "HUD stats over 5.0 s: 60.0 syncs/s, 30.0 ticks/s; mesh rebuilds per tick avg 5.2,
// max 11; sync avg 42 us, max 180 us".
[[nodiscard]] std::string FormatStats(const SyncStats& stats);

// The line where no live HUD runs (a theme without LiveHud, the phone's HUD).
inline constexpr const char* NoLiveHudStats = "HUD stats: no live HUD here (theme without LiveHud, or the phone's HUD)";
} // namespace UI::HUD::LiveHudTest
