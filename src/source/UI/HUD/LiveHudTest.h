#pragma once

#include <optional>
#include <string_view>

// `$hudtest`: local values for the live HUD (UI/HUD/LiveHud.h) in place of the hero's, so every state of the gems can
// be shown and screenshot (SD loss needs PvP). Only what the driver reads is overridden; nothing goes to the server.
//   $hudtest hp|mp|sd|ag <0..1>   a gauge's fill
//   $hudtest cd <seconds>         every skill slot cooling down for that long
//   $hudtest exp                  an EXP gain
//   $hudtest lvl                  a level-up
//   $hudtest poison on|off        the hero poisoned or not
//   $hudtest off                  the hero's own values again
// The SD and AG rings, the cooldown sweep and the EXP cues are later steps of the live HUD: until they come, their
// values are kept here and change nothing on screen.
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
    // How many EXP gains and level-ups were asked for; the driver plays one per new request.
    int expGains = 0;
    int levelUps = 0;
};

// The overrides in force this session.
Overrides& Current();

// The arguments after `$hudtest` (e.g. "hp 0.25", "poison on", "off") applied to `overrides`; false, and nothing
// changed, when they are not one of the forms above.
bool Apply(Overrides& overrides, std::wstring_view arguments);

// The reply to arguments Apply() refused.
inline constexpr const wchar_t* Usage = L"$hudtest hp|mp|sd|ag <0..1>, cd <seconds>, exp, lvl, poison on|off, off";
} // namespace UI::HUD::LiveHudTest
