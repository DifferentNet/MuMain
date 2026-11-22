#include "GameLogic/Character/GrandResetCountLine.h"

#include <cstdint>

namespace GameLogic::GrandResetCountLine
{
namespace
{
// A whole non-negative decimal number that fits an int (no sign, no spaces, at most 10 digits).
std::optional<int> ParseNumber(std::wstring_view text)
{
    if (text.empty() || text.size() > 10)
        return std::nullopt;
    std::int64_t value = 0;
    for (const wchar_t ch : text)
    {
        if (ch < L'0' || ch > L'9')
            return std::nullopt;
        value = value * 10 + (ch - L'0');
    }
    if (value > INT32_MAX)
        return std::nullopt;
    return static_cast<int>(value);
}
} // namespace

std::optional<Counts> Parse(std::wstring_view line)
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

    const std::optional<int> grandResets = ParseNumber(line.substr(0, bar));
    const std::optional<int> resets = ParseNumber(line.substr(bar + 1));
    if (!grandResets || !resets)
        return std::nullopt;

    return Counts{*grandResets, *resets};
}
} // namespace GameLogic::GrandResetCountLine
