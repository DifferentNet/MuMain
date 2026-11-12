#pragma once

#include <optional>
#include <string_view>

// The server's line with the own character's Grand stat points (MU Skynet, ghinzuro 2026-11-12: "5000, through
// /grstats"; its blue server message, hidden from the chat): "#GRS|<str>|<agi>|<vit>|<ene>|<cmd>", the points put
// into strength, agility, vitality, energy and command with /grstats (GameLogic/Items/GrandStats.h).
namespace GameLogic::Items::GrandStatsLine
{
inline constexpr std::wstring_view Prefix = L"#GRS|";

struct Points
{
    int strength = 0;
    int agility = 0;
    int vitality = 0;
    int energy = 0;
    int command = 0;

    bool operator==(const Points&) const = default;
};

// The points the line carries, or nothing when the line is not "#GRS|" and five whole numbers from 0 to 2147483647.
[[nodiscard]] std::optional<Points> Parse(std::wstring_view line);
} // namespace GameLogic::Items::GrandStatsLine
