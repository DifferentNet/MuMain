#pragma once

// TradeOfferChange.h - which changes of the partner's offer the trade window says aloud.

#include <cstdint>
#include <string>
#include <vector>

namespace UI::Trade
{
// True when the partner lowered the zen he offers. The first amount of a
// trade (0 -> N) and a rise say nothing.
[[nodiscard]] bool IsZenLowered(std::int64_t before, std::int64_t after);

// The line "X took Y out of the trade" waits a moment: when the partner only moves an item
// inside his trade box, the server sends "item gone" and then "item added" for the same item.
// An add of the same type and level within HoldMs drops the waiting line.
class PendingRemovals
{
public:
    static constexpr std::uint32_t HoldMs = 300;

    void Hold(int type, int level, std::wstring line, std::uint32_t now);
    // True when a waiting line for this item was dropped.
    bool CancelOn(int type, int level);
    // The lines whose wait is over, oldest first; they leave the list.
    [[nodiscard]] std::vector<std::wstring> TakeDue(std::uint32_t now);
    void Clear() { m_entries.clear(); }
    [[nodiscard]] bool Empty() const { return m_entries.empty(); }

private:
    struct Entry
    {
        int type;
        int level;
        std::uint32_t at;
        std::wstring line;
    };
    std::vector<Entry> m_entries;
};
} // namespace UI::Trade
