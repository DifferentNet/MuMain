#pragma once

#include <optional>
#include <string_view>

// The server's line with the own character's Grand Resets and resets (MU Skynet, ghinzuro 2026-11-22: the character
// window said "Resets: 101" after one Grand Reset; its blue server message, hidden from the chat):
// "#GRC|<grandResets>|<resets>", the answer to the hidden "/grsync" (GameLogic/Character/GrandResetCount.h).
namespace GameLogic::GrandResetCountLine
{
inline constexpr std::wstring_view Prefix = L"#GRC|";

struct Counts
{
    int grandResets = 0;
    int resets = 0;

    bool operator==(const Counts&) const = default;
};

// The counts the line carries, or nothing when the line is not "#GRC|" and two whole numbers from 0 to 2147483647.
[[nodiscard]] std::optional<Counts> Parse(std::wstring_view line);
} // namespace GameLogic::GrandResetCountLine
