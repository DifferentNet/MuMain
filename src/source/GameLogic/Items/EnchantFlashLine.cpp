#include "GameLogic/Items/EnchantFlashLine.h"

namespace GameLogic::Items::EnchantFlashLine
{
namespace
{
// A whole non-negative decimal number of at most 9 digits (no sign, no spaces).
std::optional<int> ParseNumber(std::wstring_view text)
{
    if (text.empty() || text.size() > 9)
        return std::nullopt;
    int value = 0;
    for (const wchar_t ch : text)
    {
        if (ch < L'0' || ch > L'9')
            return std::nullopt;
        value = value * 10 + (ch - L'0');
    }
    return value;
}
} // namespace

std::optional<Flash> Parse(std::wstring_view line)
{
    if (!line.starts_with(Prefix))
        return std::nullopt;
    line.remove_prefix(Prefix.size());

    // Tolerate a trailing line break or spaces the chat path may leave.
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\r' || line.back() == L'\n'))
        line.remove_suffix(1);

    const std::size_t bar = line.find(L'|');
    if (bar == std::wstring_view::npos)
        return std::nullopt;

    const std::optional<int> key = ParseNumber(line.substr(0, bar));
    const std::optional<int> level = ParseNumber(line.substr(bar + 1));
    if (!key || !level)
        return std::nullopt;
    // The client keeps character keys in a SHORT.
    if (*key > 0x7FFF)
        return std::nullopt;
    if (*level < MinLevel || *level > MaxLevel)
        return std::nullopt;
    return Flash{*key, *level};
}
} // namespace GameLogic::Items::EnchantFlashLine
