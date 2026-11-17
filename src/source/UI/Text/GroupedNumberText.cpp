#include "UI/Text/GroupedNumberText.h"

#include <algorithm>

namespace
{
bool IsLengthModifier(wchar_t c)
{
    return c == L'l' || c == L'h' || c == L'I' || c == L'6' || c == L'4' || c == L'3' || c == L'2' || c == L'z' ||
           c == L'j' || c == L't';
}

bool IsIntegerConversion(wchar_t c)
{
    return c == L'd' || c == L'i' || c == L'u';
}
} // namespace

std::wstring UI::Text::GroupThousands(std::int64_t value)
{
    std::wstring text;
    std::uint64_t magnitude = static_cast<std::uint64_t>(value);
    if (value < 0)
    {
        text.push_back(L'-');
        magnitude = 0 - magnitude;
    }

    // Commas between groups of three digits, as ConvertGold64 (ZzzInventory.cpp) writes Zen; kept free
    // of the game's globals so the unit test links it alone.
    std::wstring digits;
    int inGroup = 0;
    do
    {
        if (inGroup == 3)
        {
            digits.push_back(L',');
            inGroup = 0;
        }
        digits.push_back(static_cast<wchar_t>(L'0' + magnitude % 10));
        magnitude /= 10;
        ++inGroup;
    } while (magnitude != 0);
    std::reverse(digits.begin(), digits.end());
    text += digits;
    return text;
}

std::wstring UI::Text::FormatGroupedNumbers(const wchar_t* format, std::initializer_list<std::int64_t> values)
{
    std::wstring text;
    if (format == nullptr)
    {
        return text;
    }

    auto nextValue = values.begin();
    for (const wchar_t* p = format; *p != L'\0'; ++p)
    {
        if (*p != L'%')
        {
            text.push_back(*p);
            continue;
        }

        if (p[1] == L'%')
        {
            text.push_back(L'%');
            ++p;
            continue;
        }

        const wchar_t* conversion = p + 1;
        while (IsLengthModifier(*conversion))
        {
            ++conversion;
        }

        const bool replaceable = IsIntegerConversion(*conversion) && nextValue != values.end();
        if (!replaceable)
        {
            text.push_back(*p);
            continue;
        }

        text += GroupThousands(*nextValue);
        ++nextValue;
        p = conversion;
    }
    return text;
}
