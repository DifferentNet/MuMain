#pragma once

#include <cstdint>
#include <optional>
#include <string>

// What one MU Helper run gave, said in one chat line when the helper stops: how long it ran, the levels and
// the carried zen it gained (zen can go down: potions bought by the helper, its fee, a death). Runs shorter
// than MinRunMs say nothing.
namespace UI::Chat
{
class HelperRunSummary
{
public:
    static constexpr std::uint32_t MinRunMs = 60'000;

    struct Result
    {
        int hours = 0;
        int minutes = 0;
        // Never below 0 (a reset during the run lowers the level).
        int levels = 0;
        std::int64_t zen = 0;
    };

    void Start(std::uint32_t nowMs, int level, std::int64_t zen);
    // The run's result once (later calls give nothing until the next Start); nothing for a short run.
    [[nodiscard]] std::optional<Result> Stop(std::uint32_t nowMs, int level, std::int64_t zen);
    // Forgets the run without a result (a new game).
    void Cancel() { m_running = false; }

private:
    bool m_running = false;
    std::uint32_t m_startMs = 0;
    int m_startLevel = 0;
    std::int64_t m_startZen = 0;
};

// The zen change with its sign and grouped by thousands: "+1,250,000", "-300", "0".
[[nodiscard]] std::wstring SignedGroupedZen(std::int64_t zen);
} // namespace UI::Chat
