#pragma once

#include <cstdint>
#include <optional>

// The zen a sale, a repair or a death changed, said in one line. The server tells the client only the new amount of
// carried zen (a money update per change, plus the sale's own answer), so "Repair all" sends one update per repaired
// item and a death's loss can come just before or just after the death itself. A window sums the changes of one sign
// around the action and gives the total once they stop.
namespace UI::Chat
{
class ZenChangeWindow
{
public:
    enum class Sign
    {
        Gain,
        Loss,
    };

    struct Timing
    {
        // How far back a change before Arm() still counts (a death's loss can come before the death); 0 = none.
        std::uint32_t lookBackMs = 0;
        // How long after Arm() changes count.
        std::uint32_t windowMs = 0;
        // How long no further change must come before the total is given.
        std::uint32_t quietMs = 0;
        // How long after Arm() the first change may come; with none by then the window ends. 0 = all of windowMs.
        std::uint32_t firstWithinMs = 0;
    };

    ZenChangeWindow(Sign sign, Timing timing) : m_sign(sign), m_timing(timing) {}

    // The action started (a sale or repair was asked, the hero died). Arming again while armed (or stopped) keeps the
    // sum.
    void Arm(std::uint32_t nowMs);

    // The action was refused (no change will come for it): nothing more is counted, and a sum already made is given
    // at the next Poll().
    void Stop();

    // A change of the carried zen by `delta`. Whether it was counted for the action (its own line is then not said).
    bool Note(std::int64_t delta, std::uint32_t nowMs);

    // The summed amount (always above 0) once the changes have stopped or the window has ended; nothing while still
    // waiting or when nothing changed. Gives each total once.
    [[nodiscard]] std::optional<std::int64_t> Poll(std::uint32_t nowMs);

    [[nodiscard]] bool Armed() const { return m_armed; }

private:
    [[nodiscard]] bool Counting(std::uint32_t nowMs) const;

    Sign m_sign;
    Timing m_timing;
    bool m_armed = false;
    bool m_stopped = false;
    std::uint32_t m_armedAt = 0;
    std::uint32_t m_lastAt = 0;
    std::int64_t m_total = 0;
    std::uint32_t m_recentAt = 0;
    std::int64_t m_recent = 0;
};
} // namespace UI::Chat
