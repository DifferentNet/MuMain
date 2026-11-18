#pragma once

#include <cstdint>

// One quick slot's "running low" line (UI/HUD/ItemHotKey.cpp): when the stack count falls from above Threshold to
// Threshold or below (not to 0, which has its own line on the key press), once. It is said again only after the count
// went above Threshold again, and not within MinGapMs of the last time, so picking potions up while drinking does
// not repeat it. Another item put on the slot starts over without a line.
namespace UI::Hud
{
class LowStockWarning
{
public:
    static constexpr int Threshold = 10;
    static constexpr std::uint32_t MinGapMs = 60'000;

    // The slot's item (type and level) and its count now; whether to say it is running low.
    [[nodiscard]] bool Note(int itemType, int itemLevel, int count, std::uint32_t nowMs);
    // The same, never saying it: while the count moves for another reason (a stack on the cursor, a vault, a trade).
    void Follow(int itemType, int itemLevel, int count);

private:
    // Takes the count in; whether it just fell from above Threshold to 1..Threshold.
    bool Track(int itemType, int itemLevel, int count);

    bool m_known = false;
    int m_itemType = -1;
    int m_itemLevel = 0;
    bool m_armed = false;
    bool m_said = false;
    std::uint32_t m_saidAt = 0;
};
} // namespace UI::Hud
