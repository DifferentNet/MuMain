// What the touch controls' Social panel does with another player (UI/HUD/TouchControls): trade, a
// party or a guild invitation, a whisper, a friend request, a duel, follow, and a guild master's
// alliance and rivalry. The same commands as the PC's command window (D), without its "pick a
// command, then click the player": the panel lists the players around and one is chosen there, by a
// finger or the pad.
//
// The game sends most of these only to a player within two tiles (the command window's rule). One
// further away is walked up to first, and the request goes out on arrival.
#pragma once

#include <string>
#include <vector>

namespace UI::Mobile::Social
{
enum class Action : int
{
    Trade,
    Party,
    Whisper,
    Friend,
    Duel,
    Follow,
    // Asks to join the player's guild: they must be its master.
    Guild,
    // A guild master's, with another guild's master.
    Alliance,
    Rival,
    EndRivalry,
    Count,
};
inline constexpr int ActionCount = static_cast<int>(Action::Count);

// The action's label, as the command window has it.
[[nodiscard]] const wchar_t* Label(Action action);
// Whether the panel offers the action now: the guild ones by the character's own place in a guild.
[[nodiscard]] bool Offered(Action action);

// How far around the character the panel looks for players, in tiles.
inline constexpr int ListRange = 12;

struct Player
{
    int key = -1;
    std::wstring name;
    // The player's guild, or empty.
    std::wstring guild;

    bool operator==(const Player&) const = default;
};

// The players within ListRange, nearest first, at most `count`.
[[nodiscard]] std::vector<Player> NearbyPlayers(int count);
// Whether the player is still around (within ListRange).
[[nodiscard]] bool IsAround(int key);

// The player the attack button or the pad's pointer went for, if it is a player around; else -1.
[[nodiscard]] int AimedPlayer();

// Where the commands are refused (Chaos Castle and the places where players fight), as the command
// window is: says so and returns true.
bool RefusedHere();

// The action on the player: at once, or after walking up to them. A newer one replaces it.
void Start(Action action, int key);
// Once a frame: carries the walk on and sends the request on arrival. `steering`: the joystick or
// the pad's stick walks, which drops it.
void Update(bool steering);
// Drops the walk (the world left, the controls hidden).
void Cancel();
} // namespace UI::Mobile::Social
