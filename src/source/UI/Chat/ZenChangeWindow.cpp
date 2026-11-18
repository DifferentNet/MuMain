#include "UI/Chat/ZenChangeWindow.h"

namespace UI::Chat
{
void ZenChangeWindow::Arm(std::uint32_t nowMs)
{
    if (!m_armed)
    {
        m_armed = true;
        m_total = 0;
        m_lastAt = nowMs;
        if (m_timing.lookBackMs > 0 && m_recent > 0 && nowMs - m_recentAt <= m_timing.lookBackMs)
            m_total = m_recent;
    }
    m_armedAt = nowMs;
    m_stopped = false;
    m_recent = 0;
}

void ZenChangeWindow::Stop()
{
    if (!m_armed)
        return;
    if (m_total > 0)
        m_stopped = true;
    else
        m_armed = false;
}

bool ZenChangeWindow::Counting(std::uint32_t nowMs) const
{
    if (!m_armed || m_stopped)
        return false;
    const std::uint32_t since = nowMs - m_armedAt;
    if (since > m_timing.windowMs)
        return false;
    // Nothing came in time: the answer was lost or refused, so a later change is not the action's.
    return m_total > 0 || m_timing.firstWithinMs == 0 || since <= m_timing.firstWithinMs;
}

bool ZenChangeWindow::Note(std::int64_t delta, std::uint32_t nowMs)
{
    const std::int64_t amount = m_sign == Sign::Gain ? delta : -delta;
    if (amount <= 0)
        return false;

    if (Counting(nowMs))
    {
        m_total += amount;
        m_lastAt = nowMs;
        return true;
    }

    if (m_timing.lookBackMs > 0)
    {
        if (m_recent == 0 || nowMs - m_recentAt > m_timing.lookBackMs)
            m_recent = 0;
        m_recent += amount;
        m_recentAt = nowMs;
    }
    return false;
}

std::optional<std::int64_t> ZenChangeWindow::Poll(std::uint32_t nowMs)
{
    if (!m_armed)
        return std::nullopt;

    const bool settled = m_total > 0 && nowMs - m_lastAt >= m_timing.quietMs;
    const bool ended = m_stopped || !Counting(nowMs);
    if (!settled && !ended)
        return std::nullopt;

    m_armed = false;
    m_stopped = false;
    const std::int64_t total = m_total;
    m_total = 0;
    if (total <= 0)
        return std::nullopt;
    return total;
}
} // namespace UI::Chat
