// The keys of the gamepad's on-screen keyboard (UI/HUD/PadKeyboard): each page's rows, and where a
// step of the d-pad leads from a key. No drawing and no input here.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace UI::HUD::PadKeyboardLayout
{
enum class Page : std::uint8_t
{
    // English letters under the digits.
    Latin,
    // Russian letters (ЙЦУКЕН) under the digits.
    Cyrillic,
    // Digits and punctuation.
    Symbols,
    // A number pad, for a field that takes digits only.
    Digits,
};

enum class Action : std::uint8_t
{
    // Types its text.
    Type,
    // Capitals: pressed once for the next letter, twice to stay.
    Shift,
    Backspace,
    Space,
    Enter,
    // On to the next field.
    Tab,
    // The symbols page.
    Symbols,
    // Back to the letters.
    Letters,
    // English letters and Russian ones.
    Language,
};

struct Key
{
    Action action = Action::Type;
    // What a Type key types: small, and with shift.
    std::string_view lower;
    std::string_view upper;
    // In key widths: an ordinary key is 1.
    float width = 1.f;
};

// A key's place: its row from the top and its place in the row from the left.
struct Spot
{
    int row = 0;
    int column = 0;

    bool operator==(const Spot&) const = default;
};

// The page's rows, top to bottom.
std::span<const std::span<const Key>> Rows(Page page);
// The page's width in key widths: its rows are centred in it.
float Width(Page page);
// The key at a spot Clamp() kept on the page.
const Key& KeyAt(Page page, Spot spot);
// The spot kept on the page: the last row or key where the page has fewer.
Spot Clamp(Page page, Spot spot);
// Where a step leads: left and right along the row, up and down to the key in the next row whose
// middle is nearest. Each way goes round at the end.
Spot Step(Page page, Spot from, int dx, int dy);
// The page's first key with this action, or the first key.
Spot Find(Page page, Action action);
} // namespace UI::HUD::PadKeyboardLayout
