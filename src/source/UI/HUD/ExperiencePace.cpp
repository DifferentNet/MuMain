#include "UI/HUD/ExperiencePace.h"

#include <algorithm>
#include <cmath>

namespace UI::HUD::ExperiencePace
{
void Tracker::Add(std::uint32_t nowMs, std::int64_t gained, bool master)
{
    if (gained <= 0)
        return;

    if (master != m_master)
    {
        Reset();
        m_master = master;
    }

    Prune(nowMs);
    // After a long break the window starts again at this kill, not at the old one.
    if (m_samples.empty())
        m_start = nowMs;
    m_samples.emplace_back(nowMs, gained);
}

std::optional<double> Tracker::PerHour(std::uint32_t nowMs) const
{
    if (m_samples.empty())
        return std::nullopt;

    std::int64_t sum = 0;
    for (const auto& [at, gained] : m_samples)
    {
        if (nowMs - at <= WindowMs)
            sum += gained;
    }

    const std::uint32_t elapsed = std::min(nowMs - m_start, WindowMs);
    if (sum <= 0 || elapsed < MinimumMs)
        return std::nullopt;

    return static_cast<double>(sum) * 3'600'000.0 / static_cast<double>(elapsed);
}

void Tracker::Reset()
{
    m_samples.clear();
    m_start = 0;
}

void Tracker::Prune(std::uint32_t nowMs)
{
    while (!m_samples.empty() && nowMs - m_samples.front().first > WindowMs)
        m_samples.pop_front();
}

std::optional<std::int64_t> MinutesToGain(std::int64_t remaining, double perHour)
{
    if (remaining <= 0 || !(perHour > 0.0))
        return std::nullopt;

    const double minutes = std::ceil(static_cast<double>(remaining) * 60.0 / perHour);
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(minutes));
}

Tracker& Hero()
{
    static Tracker tracker;
    return tracker;
}
} // namespace UI::HUD::ExperiencePace
