#pragma once

#include <optional>
#include <string>
#include <string_view>

// The server's echo line, the battle pass mark «Echo» (MU Skynet, ghinzuro 2026-11-22, variant 3: violet afterimages
// of the hero and its mount). The server sends it only to clients that asked "/bpmarks 3"
// (GameLogic/Events/SeasonMarks.h):
//   "#BPE|name|season"         the newest echo the character owns and shows; season 0 = none or hidden by its owner;
//   "#BPE|name|season|hidden"  the owner line, only to the owner: the newest echo season it owns (0 = none) and its
//                              own switch, hidden 0 or 1. A line with four fields is always the owner line.
namespace GameLogic::SeasonEchoLine
{
inline constexpr std::wstring_view Prefix = L"#BPE|";

struct Line
{
    std::wstring name;
    int season = 0;
    bool owner = false;  // the four-field owner line
    bool hidden = false; // owner line only: the owner turned its echo off

    bool operator==(const Line&) const = default;
};

// The line's fields, or nothing when it is not "#BPE|", the name is empty, the season is not a whole number from 0 to
// 2147483647, the hidden flag is not 0 or 1, or it has other than three or four fields.
[[nodiscard]] std::optional<Line> Parse(std::wstring_view line);
} // namespace GameLogic::SeasonEchoLine
