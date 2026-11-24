#include "GameLogic/Events/SeasonEchoLine.h"

#include <cstdint>

namespace GameLogic::SeasonEchoLine
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

std::optional<Line> Parse(std::wstring_view line)
{
    if (!line.starts_with(Prefix))
        return std::nullopt;
    line.remove_prefix(Prefix.size());

    // Tolerate a trailing line break or spaces the chat path may leave.
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\r' || line.back() == L'\n'))
        line.remove_suffix(1);

    // After the prefix: "name|season" (the shown line) or "name|season|hidden" (the owner line).
    std::wstring_view fields[3];
    std::size_t count = 0;
    while (true)
    {
        if (count == 3)
            return std::nullopt; // more fields than the owner line has
        const std::size_t bar = line.find(L'|');
        fields[count++] = line.substr(0, bar);
        if (bar == std::wstring_view::npos)
            break;
        line.remove_prefix(bar + 1);
    }
    if (count != 2 && count != 3)
        return std::nullopt;

    if (fields[0].empty())
        return std::nullopt;
    const std::optional<int> season = ParseNumber(fields[1]);
    if (!season)
        return std::nullopt;

    Line parsed;
    parsed.name.assign(fields[0]);
    parsed.season = *season;
    if (count == 3)
    {
        if (fields[2] != L"0" && fields[2] != L"1")
            return std::nullopt;
        parsed.owner = true;
        parsed.hidden = fields[2] == L"1";
    }
    return parsed;
}
} // namespace GameLogic::SeasonEchoLine
