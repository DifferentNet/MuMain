#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The exact +14/+15 of other characters' worn items (ghinzuro 2026-11-09: «Передавать точный уровень с сервера»).
// The look packet carries an item's level in 3 bits, so +13 and +14 arrive alike (both drawn as 13) and observers saw
// a +14 without its sparks. Our server's add-on (AncientLook) sends, to a client that asked "/ancset lv", the blue line
// "#UPL|<name>|<levels>": seven characters for left hand, right hand, helm, armour, pants, gloves and boots,
// 'e' = +14, 'f' = +15, '0' = lower or empty ("0000000" when the character left or has none).
namespace GameLogic::Items::ExactLevelLine
{
inline constexpr std::wstring_view Prefix = L"#UPL|";

// Left hand, right hand, helm, armour, pants, gloves, boots.
inline constexpr int Pieces = 7;
inline constexpr int FirstArmourPiece = 2;

using Levels = std::array<std::uint8_t, Pieces>;

struct Line
{
    std::wstring name;
    Levels levels{}; // 0, 14 or 15 per piece
    bool any = false; // a piece at 14 or 15
};

// The line's name and levels, or nothing when it is not "#UPL|<name>|<7 chars of 0, e, f>".
[[nodiscard]] std::optional<Line> Parse(std::wstring_view line);

// The level to draw a piece at: the exact one when it fits what the look packet gave (+14 arrives as 13, +15 as 15),
// else the shown one (the item changed since, or the server said nothing).
[[nodiscard]] constexpr int Resolve(int shownLevel, int exactLevel)
{
    if (exactLevel == 14 && shownLevel == 13)
        return 14;
    return shownLevel;
}
} // namespace GameLogic::Items::ExactLevelLine
