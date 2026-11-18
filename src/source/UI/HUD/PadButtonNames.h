// The pad's buttons named in a text (the help's pad page, a hint): the texts say {A}, {B}, {X}, {Y}, {LB},
// {RB}, {LT}, {RT}, {LS}, {RS} (a stick pressed in), {MENU} and {VIEW}, and the names of the pad in hand
// go in their place (UI/HUD/PadGlyphs: Xbox letters, PlayStation's Cross .. L1 .., Nintendo's letters).
// The sticks by their job, in words that follow the player's layout (left-handed: the right stick walks):
// {WALKSTICK} and {AIMSTICK} as a label ("Left stick: walk"), {WALKSTICK_OBJ} and {AIMSTICK_OBJ} inside
// a sentence ("let go of the left stick"); the words come from the language (PadStickLeft ..).
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
    std::wstring walkStick = L"Left stick";
    std::wstring aimStick = L"Right stick";
    std::wstring walkStickObject = L"left stick";
    std::wstring aimStickObject = L"right stick";
};

// The text with each {NAME} it knows replaced; any other brace is left as it is.
[[nodiscard]] std::wstring Fill(std::wstring_view text, const Names& names);
} // namespace UI::HUD::PadButtonNames
