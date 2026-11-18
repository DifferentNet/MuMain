#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

// The hero's experience per hour over the last 10 minutes, for the experience bar's tooltip. Fed with each gain the
// server reports; normal and master experience are counted apart (a switch starts again).
namespace UI::HUD::ExperiencePace
{
constexpr std::uint32_t WindowMs = 10u * 60u * 1000u;
// Less than this of hunting gives no pace yet: the first kills would promise far too much.
constexpr std::uint32_t MinimumMs = 60u * 1000u;
// A time to the next level above this is not shown (only the pace is).
constexpr std::int64_t MaxShownMinutes = 99 * 60;

class Tracker
{
public:
    void Add(std::uint32_t nowMs, std::int64_t gained, bool master);

    // Experience per hour; nothing until a minute has passed since the first gain or with nothing gained in the window.
    [[nodiscard]] std::optional<double> PerHour(std::uint32_t nowMs) const;

    void Reset();

private:
    void Prune(std::uint32_t nowMs);

    std::deque<std::pair<std::uint32_t, std::int64_t>> m_samples;
    std::uint32_t m_start = 0;
    bool m_master = false;
};

// Whole minutes (at least 1) to gain `remaining` at `perHour`; nothing when the pace or the remainder is not above 0.
[[nodiscard]] std::optional<std::int64_t> MinutesToGain(std::int64_t remaining, double perHour);

// The hero's tracker.
Tracker& Hero();
} // namespace UI::HUD::ExperiencePace
