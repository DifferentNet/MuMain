#include "doctest.h"

#include "Render/Effects/PoolSlotBounds.h"

#include <array>
#include <random>

using Render::Effects::ActivePoolSlot;
using Render::Effects::PoolSlotBounds;

namespace
{
constexpr int PoolSize = 48;

struct Slot
{
    bool Live = false;
};

// A pool driven through PoolSlotBounds next to the plain scans the game used before, so every create must take the
// same slot and every live slot must sit below End().
class Pool
{
public:
    int LowestFree() const
    {
        for (int i = 0; i < PoolSize; ++i)
        {
            if (!slots[i].Live)
                return i;
        }
        return -1;
    }

    int Create()
    {
        const int expected = LowestFree();
        int taken = -1;
        for (int i = bounds.SearchStart(slots.data()); i < PoolSize; ++i)
        {
            if (!slots[i].Live)
            {
                slots[i].Live = true;
                bounds.OnTaken(i);
                taken = i;
                break;
            }
        }
        CHECK(taken == expected);
        CheckEnd();
        return taken;
    }

    void Free(int index)
    {
        slots[index].Live = false;
        bounds.OnFreed(index);
    }

    void CheckEnd() const
    {
        for (int i = bounds.End(); i < PoolSize; ++i)
            CHECK_FALSE(slots[i].Live);
    }

    std::array<Slot, PoolSize> slots{};
    PoolSlotBounds bounds;
};
} // namespace

TEST_CASE("A pool takes the lowest free slot through frees, kills and spawns in moves [effects][pool_slot_bounds]")
{
    Pool pool;
    std::mt19937 random(1234);
    auto roll = [&random](int sides) { return static_cast<int>(random() % static_cast<unsigned>(sides)); };

    for (int frame = 0; frame < 400; ++frame)
    {
        const int creates = roll(8);
        for (int k = 0; k < creates; ++k)
            pool.Create();

        // Code outside the moves frees a few slots and says so.
        if (roll(3) == 0)
        {
            const int index = roll(PoolSize);
            if (pool.slots[index].Live)
                pool.Free(index);
        }

        // The move pass: a slot may end itself and spawn more in the same body, as particles and joints do.
        pool.bounds.TrimEnd(pool.slots.data(), PoolSize);
        pool.CheckEnd();
        for (int i = 0; i < pool.bounds.End(); ++i)
        {
            if (!pool.slots[i].Live)
                continue;
            const ActivePoolSlot<Slot> active(pool.bounds, pool.slots.data(), i);
            if (roll(4) == 0)
                pool.slots[i].Live = false;
            if (roll(5) == 0)
                pool.Create();
        }
        pool.CheckEnd();
    }
}

TEST_CASE("A create that ends its own slot and starts another leaves the slot to the next create [effects][pool_slot_bounds]")
{
    Pool pool;
    const int outer = pool.Create();
    {
        const ActivePoolSlot<Slot> active(pool.bounds, pool.slots.data(), outer);
        const int nested = pool.Create();
        CHECK(nested == outer + 1);
        pool.slots[outer].Live = false;
        CHECK(pool.Create() == outer);
        pool.slots[outer].Live = false;
    }
    CHECK(pool.Create() == outer);
}

TEST_CASE("Clearing every slot and noting slot 0 starts the next create at slot 0 [effects][pool_slot_bounds]")
{
    Pool pool;
    for (int k = 0; k < 10; ++k)
        pool.Create();
    for (Slot& slot : pool.slots)
        slot.Live = false;
    pool.bounds.OnFreed(0);
    CHECK(pool.Create() == 0);
    pool.bounds.TrimEnd(pool.slots.data(), PoolSize);
    CHECK(pool.bounds.End() == 1);
}
