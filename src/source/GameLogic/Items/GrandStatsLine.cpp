#include "GameLogic/Items/GrandStatsLine.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace GameLogic::Items::GrandStatsLine
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

std::optional<Points> Parse(std::wstring_view line)
{
    if (!line.starts_with(Prefix))
        return std::nullopt;
    line.remove_prefix(Prefix.size());

    // Tolerate a trailing line break or spaces the chat path may leave.
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\r' || line.back() == L'\n'))
        line.remove_suffix(1);

    std::array<int, 5> values{};
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const std::size_t bar = line.find(L'|');
        const bool last = i + 1 == values.size();
        if (last != (bar == std::wstring_view::npos))
            return std::nullopt;

        const std::optional<int> value = ParseNumber(last ? line : line.substr(0, bar));
        if (!value)
            return std::nullopt;
        values[i] = *value;
        if (!last)
            line.remove_prefix(bar + 1);
    }

    return Points{values[0], values[1], values[2], values[3], values[4]};
}
} // namespace GameLogic::Items::GrandStatsLine
