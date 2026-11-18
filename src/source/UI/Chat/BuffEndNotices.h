#pragma once

#include <cstdint>
#include <vector>

// "Buff ended" lines for the hero's own buffs (UI/Chat/HeroStatusLines.h). The server takes a buff off with a packet
// of its own; a re-cast can come as an end and a new start, a death and a map change end every buff at once. So an
// ended buff waits DelayMs, a new start of the same buff cancels it, and buffs that end together give one line once
// no further end came for DelayMs. A map change silences the ends for QuietAfterMapMs.
namespace UI::Chat
{
class BuffEndNotices
{
public:
    static constexpr std::uint32_t DelayMs = 500;
    // A stream of ends that never pauses still gives its line after this long.
    static constexpr std::uint32_t LongestWaitMs = 2'000;
    static constexpr std::uint32_t QuietAfterMapMs = 3'000;

    // A buff of the hero ended (times are milliseconds of one wrapping 32-bit clock).
    void Ended(int buff, std::uint32_t nowMs);
    // A buff of the hero started: its pending end is dropped.
    void Started(int buff);
    // Drops every pending end (the hero died).
    void Clear();
    // The hero entered a map: drops every pending end, and ends in the next QuietAfterMapMs are not said.
    void Silence(std::uint32_t nowMs);

    // The buffs to say now, in the order they ended; empty while the ends may still change.
    [[nodiscard]] std::vector<int> TakeDue(std::uint32_t nowMs);

private:
    std::vector<int> m_pending;
    std::uint32_t m_firstEndAt = 0;
    std::uint32_t m_lastEndAt = 0;
    bool m_quiet = false;
    std::uint32_t m_quietUntil = 0;
};
} // namespace UI::Chat
