#include "doctest.h"

#include "Render/Effects/FramePow.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

using Render::Effects::FramePowMemo;

namespace
{
bool SameBits(float a, float b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

// The bases the effect movers use most, written as in the code.
const std::array<float, 12> Bases = {1.0f / (1.1f), 1.0f / (1.02f), 1.0f / (1.05f), 0.95f, 0.9f, 1.0f / (1.01f),
                                     1.1f, 1.0f / (1.04f), 1.0f / (1.03f), 1.f / 1.2f, 0.001f, 1.5f};
const std::array<float, 7> Exponents = {0.f, 0.0625f, 0.25f, 1.f / 3.f, 0.5f, 0.999f, 1.f};
} // namespace

TEST_CASE("FramePow memo gives the same bits as powf")
{
    FramePowMemo memo;
    for (int round = 0; round < 2; ++round) // the second round is served from the memo
    {
        for (float exponent : Exponents)
        {
            for (float base : Bases)
            {
                CHECK(SameBits(memo.Get(base, exponent), powf(base, exponent)));
                CHECK(SameBits(memo.Get(base, exponent), powf(base, exponent)));
            }
        }
    }
}

// Every base the effect code passes to FramePow, written as in the code. With the old 128-slot table these
// pairs shared a slot and threw each other out: 1/1.05 with 1.05 and 0.92, 0.97 with 1.1, 0.9 with 1/1.16,
// 1/1.02 with 1/1.15.
const float AllBases[] = {
    1.0f / (1.1f),   1.0f / (1.02f),  1.0f / (1.05f),  0.95f,           0.9f,           0.8f,
    0.97f,           1.1f,            1.0f / (1.01f),  1.0f / (1.04f),  1.0f / (1.2f),  0.6f,
    1.0f / (1.08f),  1.0f / (1.03f),  0.5f,            1.0f / (1.3f),   1.05f,          0.98f,
    1.f / 1.2f,      1.2f,            0.7f,            0.4f,            1.f / 1.3f,     1.f / 1.1f,
    1.0f / (1.5f),   1.03f,           0.92f,           1.0f / (1.17f),  1.0f / (1.07f), 1.0f / (1.012f),
    1.0f / (1.16f),  1.f / 1.15f,     0.45f,           1.0f / (1.007f), 0.96f,          0.85f,
    0.93f,           1.02f,           1.04f,           1.15f,           1.015f,         1.16f,
    1.0f / (1.13f),  1.0f / (1.6f),   0.9978f,         1.08f,           1.24f,          1.3f,
    1.25f,           0.94f,           0.86f,           0.001f,          0.99f,          1.01f,
    1.0f / (1.008f), 1.0f / (1.35f),  0.65f,           1.07f,           1.5f,
};

TEST_CASE("FramePow memo keeps every base the code uses, frame after frame")
{
    FramePowMemo memo;
    for (float exponent : Exponents)
    {
        for (float base : AllBases) // the frame's first calls fill the memo
            CHECK(SameBits(memo.Get(base, exponent), powf(base, exponent)));
        const std::uint32_t filled = memo.Misses();
        for (int round = 0; round < 3; ++round) // the rest of the frame is served from it
        {
            for (float base : AllBases)
                CHECK(SameBits(memo.Get(base, exponent), powf(base, exponent)));
        }
        CHECK(memo.Misses() == filled);
    }
}

TEST_CASE("FramePow memo follows a new exponent")
{
    FramePowMemo memo;
    const float base = 0.95f;
    CHECK(SameBits(memo.Get(base, 0.5f), powf(base, 0.5f)));
    CHECK(SameBits(memo.Get(base, 0.25f), powf(base, 0.25f)));
    CHECK(SameBits(memo.Get(base, 0.5f), powf(base, 0.5f)));
}

TEST_CASE("FramePow memo does not hand a fresh slot's zero to base 0 at exponent 0")
{
    FramePowMemo memo;
    CHECK(SameBits(memo.Get(0.f, 0.f), powf(0.f, 0.f)));
}

TEST_CASE("FramePowFor matches powf")
{
    for (float base : Bases)
        CHECK(SameBits(Render::Effects::FramePowFor(base, 0.37f), powf(base, 0.37f)));
}
