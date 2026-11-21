#pragma once

#include <optional>
#include <string_view>

// `$hudtest`: local values for the live HUD (UI/HUD/LiveHud.h) in place of the hero's, so every state of the gems, the
// skill slots and the EXP bar can be shown and screenshot (SD loss needs PvP). Only what the driver reads is
// overridden; nothing goes to the server.
//   $hudtest hp|mp|sd|ag <0..1>   a gauge's fill
//   $hudtest cd <seconds>         every skill slot cooling down for that long, from now
//   $hudtest exp                  an EXP gain
//   $hudtest lvl                  a level-up (the hero's level shown one higher than the last)
//   $hudtest poison on|off        the hero poisoned or not
//   $hudtest off                  the hero's own values again
namespace UI::HUD::LiveHudTest
{
struct Overrides
{
    std::optional<float> life;
    std::optional<float> mana;
    std::optional<float> shield;
    std::optional<float> skillMana;
    std::optional<bool> poisoned;
    std::optional<float> cooldownSeconds;
    // How many cooldowns, EXP gains and level-ups were asked for; the driver plays one per new request.
    int cooldowns = 0;
    int expGains = 0;
    int levelUps = 0;
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
inline constexpr const wchar_t* Usage = L"$hudtest hp|mp|sd|ag <0..1>, cd <seconds>, exp, lvl, poison on|off, off";
} // namespace UI::HUD::LiveHudTest
