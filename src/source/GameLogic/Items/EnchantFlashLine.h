#pragma once

#include <optional>
#include <string_view>

// The server's line after a successful enchant to +14 or +15 (ghinzuro 2026-11-09: only +14 and +15; its blue server message, hidden from the
// chat): "#UPF|<object id>|<level>". The object id is the character's key in the client.
namespace GameLogic::Items::EnchantFlashLine
{
inline constexpr std::wstring_view Prefix = L"#UPF|";
inline constexpr int MinLevel = 14;
inline constexpr int MaxLevel = 15;

struct Flash
{
    int key = 0;
    int level = 0;
};

// The flash the line asks for, or nothing when the line is not "#UPF|<id>|<14..15>".
[[nodiscard]] std::optional<Flash> Parse(std::wstring_view line);
} // namespace GameLogic::Items::EnchantFlashLine
