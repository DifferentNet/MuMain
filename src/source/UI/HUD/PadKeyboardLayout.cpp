#include "UI/HUD/PadKeyboardLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace UI::HUD::PadKeyboardLayout
{
namespace
{
using A = Action;

constexpr Key T(std::string_view lower, std::string_view upper)
{
    return {A::Type, lower, upper, 1.f};
}
constexpr Key T(std::string_view text)
{
    return {A::Type, text, text, 1.f};
}
constexpr Key Do(Action action, float width)
{
    return {action, {}, {}, width};
}
constexpr Key Wide(std::string_view text, float width)
{
    return {A::Type, text, text, width};
}

constexpr Key LatinDigits[] = {T("1"), T("2"), T("3"), T("4"), T("5"), T("6"), T("7"), T("8"), T("9"), T("0")};
constexpr Key LatinTop[] = {T("q", "Q"), T("w", "W"), T("e", "E"), T("r", "R"), T("t", "T"),
                            T("y", "Y"), T("u", "U"), T("i", "I"), T("o", "O"), T("p", "P")};
constexpr Key LatinHome[] = {T("a", "A"), T("s", "S"), T("d", "D"), T("f", "F"), T("g", "G"),
                             T("h", "H"), T("j", "J"), T("k", "K"), T("l", "L")};
constexpr Key LatinBottom[] = {Do(A::Shift, 1.5f), T("z", "Z"), T("x", "X"), T("c", "C"),           T("v", "V"),
                               T("b", "B"),        T("n", "N"), T("m", "M"), Do(A::Backspace, 1.5f)};
constexpr Key LatinSpace[] = {
    Do(A::Symbols, 1.5f), Do(A::Language, 1.25f), Do(A::Tab, 1.25f), Do(A::Space, 3.f), T("."), Do(A::Enter, 2.f)};
constexpr std::span<const Key> LatinRows[] = {LatinDigits, LatinTop, LatinHome, LatinBottom, LatinSpace};

constexpr Key CyrillicDigits[] = {T("ё", "Ё"), T("1"), T("2"), T("3"), T("4"), T("5"),
                                  T("6"),      T("7"), T("8"), T("9"), T("0"), T("-")};
constexpr Key CyrillicTop[] = {T("й", "Й"), T("ц", "Ц"), T("у", "У"), T("к", "К"), T("е", "Е"), T("н", "Н"),
                               T("г", "Г"), T("ш", "Ш"), T("щ", "Щ"), T("з", "З"), T("х", "Х"), T("ъ", "Ъ")};
constexpr Key CyrillicHome[] = {T("ф", "Ф"), T("ы", "Ы"), T("в", "В"), T("а", "А"), T("п", "П"), T("р", "Р"),
                                T("о", "О"), T("л", "Л"), T("д", "Д"), T("ж", "Ж"), T("э", "Э")};
constexpr Key CyrillicBottom[] = {Do(A::Shift, 1.5f), T("я", "Я"), T("ч", "Ч"),           T("с", "С"),
                                  T("м", "М"),        T("и", "И"), T("т", "Т"),           T("ь", "Ь"),
                                  T("б", "Б"),        T("ю", "Ю"), Do(A::Backspace, 1.5f)};
constexpr Key CyrillicSpace[] = {
    Do(A::Symbols, 2.f), Do(A::Language, 1.5f), Do(A::Tab, 1.5f), Do(A::Space, 4.f), T("."), Do(A::Enter, 2.f)};
constexpr std::span<const Key> CyrillicRows[] = {CyrillicDigits, CyrillicTop, CyrillicHome, CyrillicBottom,
                                                 CyrillicSpace};

constexpr Key SymbolsTop[] = {T("@"), T("#"), T("$"), T("%"), T("&"), T("*"), T("-"), T("+"), T("("), T(")")};
constexpr Key SymbolsHome[] = {T("!"), T("\""), T("'"), T(":"), T(";"), T("/"), T("?"), T("_"), T("=")};
constexpr Key SymbolsBottom[] = {
    T("~"), T("^"), T("\\"), T("|"), T("<"), T(">"), T("["), T("]"), Do(A::Backspace, 2.f)};
constexpr Key SymbolsSpace[] = {Do(A::Letters, 1.5f), Wide(",", 1.25f), Do(A::Tab, 1.25f),
                                Do(A::Space, 3.f),    T("."),           Do(A::Enter, 2.f)};
constexpr std::span<const Key> SymbolsRows[] = {LatinDigits, SymbolsTop, SymbolsHome, SymbolsBottom, SymbolsSpace};

constexpr Key DigitsTop[] = {T("1"), T("2"), T("3")};
constexpr Key DigitsMiddle[] = {T("4"), T("5"), T("6")};
constexpr Key DigitsLow[] = {T("7"), T("8"), T("9")};
constexpr Key DigitsBottom[] = {Do(A::Backspace, 1.f), T("0"), Do(A::Enter, 1.f)};
constexpr std::span<const Key> DigitsRows[] = {DigitsTop, DigitsMiddle, DigitsLow, DigitsBottom};

float RowWidth(std::span<const Key> row)
{
    float width = 0.f;
    for (const Key& key : row)
        width += key.width;
    return width;
}

// The middle of a key from the page's left edge, in key widths.
float Middle(Page page, Spot spot)
{
    const std::span<const Key> row = Rows(page)[spot.row];
    float left = (Width(page) - RowWidth(row)) / 2.f;
    for (int column = 0; column < spot.column; ++column)
        left += row[column].width;
    return left + row[spot.column].width / 2.f;
}

int Wrap(int value, int count)
{
    return (value % count + count) % count;
}

// The key in a row whose middle is nearest to a point; the left one of two as near.
int NearestColumn(Page page, int row, float middle)
{
    const int count = static_cast<int>(Rows(page)[row].size());
    int nearest = 0;
    float nearestDistance = std::numeric_limits<float>::max();
    for (int column = 0; column < count; ++column)
    {
        const float distance = std::abs(Middle(page, {row, column}) - middle);
        if (distance < nearestDistance)
        {
            nearest = column;
            nearestDistance = distance;
        }
    }
    return nearest;
}
} // namespace

std::span<const std::span<const Key>> Rows(Page page)
{
    switch (page)
    {
    case Page::Cyrillic:
        return CyrillicRows;
    case Page::Symbols:
        return SymbolsRows;
    case Page::Digits:
        return DigitsRows;
    default:
        return LatinRows;
    }
}

float Width(Page page)
{
    float width = 0.f;
    for (const std::span<const Key> row : Rows(page))
        width = std::max(width, RowWidth(row));
    return width;
}

Spot Clamp(Page page, Spot spot)
{
    const auto rows = Rows(page);
    const int row = std::clamp(spot.row, 0, static_cast<int>(rows.size()) - 1);
    const int column = std::clamp(spot.column, 0, static_cast<int>(rows[row].size()) - 1);
    return {row, column};
}

const Key& KeyAt(Page page, Spot spot)
{
    const Spot kept = Clamp(page, spot);
    return Rows(page)[kept.row][kept.column];
}

Spot Step(Page page, Spot from, int dx, int dy)
{
    const Spot spot = Clamp(page, from);
    const auto rows = Rows(page);
    if (dy != 0)
    {
        const int row = Wrap(spot.row + dy, static_cast<int>(rows.size()));
        return {row, NearestColumn(page, row, Middle(page, spot))};
    }
    if (dx != 0)
        return {spot.row, Wrap(spot.column + dx, static_cast<int>(rows[spot.row].size()))};
    return spot;
}

Spot Find(Page page, Action action)
{
    const auto rows = Rows(page);
    for (int row = 0; row < static_cast<int>(rows.size()); ++row)
    {
        for (int column = 0; column < static_cast<int>(rows[row].size()); ++column)
        {
            if (rows[row][column].action == action)
                return {row, column};
        }
    }
    return {};
}
} // namespace UI::HUD::PadKeyboardLayout
