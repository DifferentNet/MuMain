#pragma once

#include <algorithm>
#include <array>
#include <cassert>

namespace Render::Effects
{
// Cuts the scans of a fixed effect pool (sprites, particles, joints) whose create takes the lowest free slot,
// without changing which slot a create takes or which live slots a loop visits:
// - End(): one past the highest slot that may be live. Every slot from it on is free, so loops stop there.
// - SearchStart(): where a create looks for the lowest free slot. Every slot below the hint is live, except the
//   slots whose move or create is running right now (they may have died in it), which are checked on their own.
// The pool's create calls OnTaken when it sets a slot live. Whoever sets a slot's Live to false outside a running
// move or create calls OnFreed (OnFreed(0) after clearing many slots at once).
class PoolSlotBounds
{
public:
    [[nodiscard]] int End() const { return m_end; }

    template <typename Slot>
    [[nodiscard]] int SearchStart(const Slot* slots) const
    {
        if (m_activeCount > MaxActive)
            return 0;

        int start = m_firstFree;
        for (int k = 0; k < m_activeCount; ++k)
        {
            const int active = m_active[k];
            if (active < start && !slots[active].Live)
                start = active;
        }
        return start;
    }

    // The create set `index` live; it was the lowest free slot, so every slot below it is live.
    void OnTaken(int index)
    {
        m_firstFree = index;
        m_end = std::max(m_end, index + 1);
    }

    void OnFreed(int index) { m_firstFree = std::min(m_firstFree, index); }

    // Moves End() down past the free slots at the top; right at any time. `capacity` is the pool's size.
    template <typename Slot>
    void TrimEnd(const Slot* slots, [[maybe_unused]] int capacity)
    {
#ifndef NDEBUG
        // Debug builds only: a slot set live without OnTaken would be skipped by every loop.
        for (int i = m_end; i < capacity; ++i)
            assert(!slots[i].Live && "pool slot live at or above End(): its create did not call OnTaken");
#endif
        while (m_end > 0 && !slots[m_end - 1].Live)
            --m_end;
    }

    // Brackets the move or create of one slot: its body may set the slot's Live to false and then create more.
    void Enter(int index)
    {
        if (m_activeCount < MaxActive)
            m_active[m_activeCount] = index;
        ++m_activeCount;
    }

    template <typename Slot>
    void Leave(const Slot* slots, int index)
    {
        --m_activeCount;
        if (!slots[index].Live)
            OnFreed(index);
    }

private:
    // Moves and creates nest only a few deep; past this SearchStart falls back to slot 0, which is always right.
    static constexpr int MaxActive = 16;

    int m_firstFree = 0;
    int m_end = 0;
    std::array<int, MaxActive> m_active{};
    int m_activeCount = 0;
};

// Enter/Leave for one scope, so every return and continue of the body leaves the slot.
template <typename Slot>
class ActivePoolSlot
{
public:
    ActivePoolSlot(PoolSlotBounds& bounds, const Slot* slots, int index)
        : m_bounds(bounds), m_slots(slots), m_index(index)
    {
        m_bounds.Enter(m_index);
    }
    ~ActivePoolSlot() { m_bounds.Leave(m_slots, m_index); }

    ActivePoolSlot(const ActivePoolSlot&) = delete;
    ActivePoolSlot& operator=(const ActivePoolSlot&) = delete;

private:
    PoolSlotBounds& m_bounds;
    const Slot* m_slots;
    int m_index;
};
} // namespace Render::Effects
