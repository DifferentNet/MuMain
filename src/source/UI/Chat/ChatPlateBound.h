#pragma once

#include <algorithm>

namespace UI::Chat
{
// One past the highest chat plate slot that may still be counting down, so the per-frame walks over the plates
// (move, render, HasPlate) stop there instead of walking the whole pool. Creates may write any slot: each one
// raises the end with OnUsed. The move trims it past the idle slots at the top; an idle slot is one whose
// per-frame walk changes nothing that is read later (every lifetime run out).
class ChatPlateBound
{
public:
    [[nodiscard]] int End() const { return m_end; }

    void OnUsed(int index) { m_end = std::max(m_end, index + 1); }

    template <typename Slot, typename IsIdle>
    void TrimEnd(const Slot* slots, IsIdle isIdle)
    {
        while (m_end > 0 && isIdle(slots[m_end - 1]))
            --m_end;
    }

private:
    int m_end = 0;
};
} // namespace UI::Chat
