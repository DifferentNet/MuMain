#include "UI/Chat/BuffEndNotices.h"

#include <algorithm>

namespace UI::Chat
{
namespace
{
// Whether `now` is at or after `due` on a wrapping 32-bit millisecond clock.
bool Reached(std::uint32_t now, std::uint32_t due)
{
    return static_cast<std::int32_t>(now - due) >= 0;
}
} // namespace

void BuffEndNotices::Ended(int buff, std::uint32_t nowMs)
{
    if (m_quiet && !Reached(nowMs, m_quietUntil))
        return;
    m_quiet = false;

    if (std::find(m_pending.begin(), m_pending.end(), buff) != m_pending.end())
        return;
    if (m_pending.empty())
        m_firstEndAt = nowMs;
    m_lastEndAt = nowMs;
    m_pending.push_back(buff);
}

void BuffEndNotices::Started(int buff)
{
    m_pending.erase(std::remove(m_pending.begin(), m_pending.end(), buff), m_pending.end());
}

void BuffEndNotices::Clear()
{
    m_pending.clear();
}

void BuffEndNotices::Silence(std::uint32_t nowMs)
{
    m_pending.clear();
    m_quiet = true;
    m_quietUntil = nowMs + QuietAfterMapMs;
}

std::vector<int> BuffEndNotices::TakeDue(std::uint32_t nowMs)
{
    if (m_pending.empty())
        return {};
    const bool settled = Reached(nowMs, m_lastEndAt + DelayMs);
    const bool waitedLongest = Reached(nowMs, m_firstEndAt + LongestWaitMs);
    if (!settled && !waitedLongest)
        return {};
    std::vector<int> due;
    due.swap(m_pending);
    return due;
}
} // namespace UI::Chat
