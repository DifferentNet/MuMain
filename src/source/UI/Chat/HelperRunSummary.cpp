#include "UI/Chat/HelperRunSummary.h"

#include "UI/Text/GroupedNumberText.h"

#include <algorithm>

namespace UI::Chat
{
namespace
{
constexpr std::uint32_t MsPerMinute = 60'000;
constexpr std::uint32_t MinutesPerHour = 60;
} // namespace

void HelperRunSummary::Start(std::uint32_t nowMs, int level, std::int64_t zen)
{
    m_running = true;
    m_startMs = nowMs;
    m_startLevel = level;
    m_startZen = zen;
}

std::optional<HelperRunSummary::Result> HelperRunSummary::Stop(std::uint32_t nowMs, int level, std::int64_t zen)
{
    if (!m_running)
        return std::nullopt;
    m_running = false;

    // Unsigned subtraction keeps the length right across the tick counter's wrap.
    const std::uint32_t lengthMs = nowMs - m_startMs;
    if (lengthMs < MinRunMs)
        return std::nullopt;

    const std::uint32_t totalMinutes = lengthMs / MsPerMinute;
    Result result;
    result.hours = static_cast<int>(totalMinutes / MinutesPerHour);
    result.minutes = static_cast<int>(totalMinutes % MinutesPerHour);
    result.levels = std::max(0, level - m_startLevel);
    result.zen = zen - m_startZen;
    return result;
}

std::wstring SignedGroupedZen(std::int64_t zen)
{
    const std::wstring grouped = UI::Text::GroupThousands(zen);
    return zen > 0 ? L"+" + grouped : grouped;
}
} // namespace UI::Chat
