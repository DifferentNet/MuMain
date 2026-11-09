#include "GameLogic/Items/ExactLevelLine.h"

namespace GameLogic::Items::ExactLevelLine
{
std::optional<Line> Parse(std::wstring_view text)
{
    if (!text.starts_with(Prefix))
        return std::nullopt;
    text.remove_prefix(Prefix.size());

    // Tolerate a trailing line break or spaces the chat path may leave.
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\r' || text.back() == L'\n'))
        text.remove_suffix(1);

    const std::size_t bar = text.find(L'|');
    if (bar == std::wstring_view::npos || bar == 0)
        return std::nullopt;
    const std::wstring_view levels = text.substr(bar + 1);
    if (levels.size() != Pieces)
        return std::nullopt;

    Line line;
    line.name.assign(text.substr(0, bar));
    for (int i = 0; i < Pieces; ++i)
    {
        std::uint8_t level = 0;
        switch (levels[static_cast<std::size_t>(i)])
        {
        case L'0':
            break;
        case L'e':
            level = 14;
            break;
        case L'f':
            level = 15;
            break;
        default:
            return std::nullopt;
        }
        line.levels[static_cast<std::size_t>(i)] = level;
        line.any = line.any || level != 0;
    }
    return line;
}
} // namespace GameLogic::Items::ExactLevelLine
