#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

extern float FPS_ANIMATION_FACTOR;

namespace Render::Effects
{
// powf(base, exponent) with a small memo. The effect movers raise a few dozen constant bases to the same
// per-frame exponent (FPS_ANIMATION_FACTOR) thousands of times a frame; the memo hands back the value powf gave
// for the same base and exponent bits, so every result is bit-identical to calling powf directly.
// The memo is per thread (animation workers may call it too): 512 sets of two entries, so the bases the code
// uses (65 in all, at most two to a set) keep their entries; one past two in a set just calls powf again.
class FramePowMemo
{
public:
    [[nodiscard]] float Get(float base, float exponent)
    {
        const std::uint32_t baseBits = Bits(base);
        const std::uint32_t exponentBits = Bits(exponent);
        Set& set = m_sets[SetOf(baseBits)];
        for (const Entry& entry : set.ways)
        {
            if (entry.filled && entry.baseBits == baseBits && entry.exponentBits == exponentBits)
                return entry.value;
        }

        ++m_misses;
        // A new exponent (the next frame) refills the base's own entry; a new base takes an empty one, or the
        // older of the two.
        Entry* target = nullptr;
        for (Entry& entry : set.ways)
        {
            if (entry.filled && entry.baseBits == baseBits)
                target = &entry;
        }
        if (target == nullptr)
        {
            for (Entry& entry : set.ways)
            {
                if (!entry.filled && target == nullptr)
                    target = &entry;
            }
        }
        if (target == nullptr)
        {
            target = &set.ways[set.older];
            set.older = static_cast<std::uint8_t>(1 - set.older);
        }
        target->value = powf(base, exponent);
        target->baseBits = baseBits;
        target->exponentBits = exponentBits;
        target->filled = true;
        return target->value;
    }

    // How many calls powf answered (the test checks the memo keeps the code's bases).
    [[nodiscard]] std::uint32_t Misses() const { return m_misses; }

private:
    static constexpr std::size_t SetCount = 512;
    static constexpr std::uint32_t HashMultiplier = 2654435761u;
    static constexpr int HashShift = 23; // 32 - log2(SetCount)

    struct Entry
    {
        std::uint32_t baseBits = 0;
        std::uint32_t exponentBits = 0;
        float value = 0.f;
        bool filled = false;
    };

    struct Set
    {
        std::array<Entry, 2> ways{};
        std::uint8_t older = 0;
    };

    static std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    static std::size_t SetOf(std::uint32_t baseBits) { return (baseBits * HashMultiplier) >> HashShift; }

    std::array<Set, SetCount> m_sets{};
    std::uint32_t m_misses = 0;
};

[[nodiscard]] inline float FramePowFor(float base, float exponent)
{
    thread_local FramePowMemo memo;
    return memo.Get(base, exponent);
}
} // namespace Render::Effects

// powf(base, FPS_ANIMATION_FACTOR) for a constant base, from the per-frame memo.
[[nodiscard]] inline float FramePow(float base)
{
    return Render::Effects::FramePowFor(base, FPS_ANIMATION_FACTOR);
}
