// The pad's buttons named in a text (the help's pad page, a hint): the texts say {A}, {B}, {X}, {Y}, {LB},
// {RB}, {LT}, {RT}, {LS}, {RS} (a stick pressed in), {MENU} and {VIEW}, and the names of the pad in hand
// go in their place (UI/HUD/PadGlyphs: Xbox letters, PlayStation's Cross .. L1 .., Nintendo's letters).
#pragma once

#include <string>
#include <string_view>

namespace UI::HUD::PadButtonNames
{
// By place, as the game reads every pad: a is the bottom face button, b the right, x the left, y the top.
struct Names
{
    std::wstring a = L"A";
    std::wstring b = L"B";
    std::wstring x = L"X";
    std::wstring y = L"Y";
    std::wstring lb = L"LB";
    std::wstring rb = L"RB";
    std::wstring lt = L"LT";
    std::wstring rt = L"RT";
    std::wstring ls = L"LS";
    std::wstring rs = L"RS";
    std::wstring menu = L"Menu";
    std::wstring view = L"View";
};

// The text with each {NAME} it knows replaced; any other brace is left as it is.
[[nodiscard]] std::wstring Fill(std::wstring_view text, const Names& names);
} // namespace UI::HUD::PadButtonNames
