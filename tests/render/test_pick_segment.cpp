#include <doctest.h>

#include "Render/Terrain/PickSegment.h"

#include <cstdint>

using Render::Terrain::Pick::Segment2D;
using Render::Terrain::Pick::SegmentCrossesBox;

namespace
{
// A fixed-seed generator, so a failure repeats.
struct Lcg
{
    std::uint32_t state = 2024u;
    float Next(float lo, float hi)
    {
        state = state * 1664525u + 1013904223u;
        const float unit = static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
        return lo + (hi - lo) * unit;
    }
};

// The reference: whether one of many points along the segment lies in the box.
bool SampledCrossesBox(const Segment2D& s, float minX, float minY, float maxX, float maxY)
{
    constexpr int Steps = 4000;
    for (int i = 0; i <= Steps; ++i)
    {
        const float t = static_cast<float>(i) / Steps;
        const float x = s.startX + (s.endX - s.startX) * t;
        const float y = s.startY + (s.endY - s.startY) * t;
        if (x >= minX && x <= maxX && y >= minY && y <= maxY)
        {
            return true;
        }
    }
    return false;
}
} // namespace

TEST_CASE("pick segment against a block box: plain cases")
{
    const Segment2D across{-10.f, 5.f, 30.f, 5.f};
    CHECK(SegmentCrossesBox(across, 0.f, 0.f, 10.f, 10.f));
    CHECK_FALSE(SegmentCrossesBox(across, 0.f, 6.f, 10.f, 10.f));

    const Segment2D shortOfBox{-10.f, 5.f, -1.f, 5.f};
    CHECK_FALSE(SegmentCrossesBox(shortOfBox, 0.f, 0.f, 10.f, 10.f));

    const Segment2D inside{2.f, 2.f, 3.f, 4.f};
    CHECK(SegmentCrossesBox(inside, 0.f, 0.f, 10.f, 10.f));

    // A vertical mouse ray projects to a point.
    const Segment2D point{4.f, 4.f, 4.f, 4.f};
    CHECK(SegmentCrossesBox(point, 0.f, 0.f, 10.f, 10.f));
    CHECK_FALSE(SegmentCrossesBox(point, 5.f, 0.f, 10.f, 10.f));

    // Passing by a corner diagonally.
    const Segment2D diagonal{0.f, 20.f, 20.f, 0.f};
    CHECK(SegmentCrossesBox(diagonal, 0.f, 0.f, 10.f, 10.f));
    CHECK_FALSE(SegmentCrossesBox(diagonal, 0.f, 0.f, 9.f, 9.f));
}

TEST_CASE("pick segment against a block box agrees with sampling the segment")
{
    Lcg random;
    int crossed = 0;
    for (int i = 0; i < 20000; ++i)
    {
        const Segment2D s{random.Next(0.f, 2000.f), random.Next(0.f, 2000.f), random.Next(0.f, 2000.f),
                          random.Next(0.f, 2000.f)};
        const float minX = random.Next(0.f, 1800.f);
        const float minY = random.Next(0.f, 1800.f);
        const float size = random.Next(100.f, 600.f);
        const bool exact = SegmentCrossesBox(s, minX, minY, minX + size, minY + size);
        const bool sampled = SampledCrossesBox(s, minX, minY, minX + size, minY + size);
        // Sampling can only miss a crossing (a corner clipped between two samples), never invent one.
        if (sampled)
        {
            CHECK(exact);
        }
        crossed += exact ? 1 : 0;
    }
    CHECK(crossed > 1000);
}

TEST_CASE("every point of the pick segment lies in a box that the test accepts")
{
    Lcg random;
    constexpr float Tile = 100.f;
    for (int i = 0; i < 5000; ++i)
    {
        const Segment2D s{random.Next(0.f, 25600.f), random.Next(0.f, 25600.f), random.Next(0.f, 25600.f),
                          random.Next(0.f, 25600.f)};
        const float t = random.Next(0.f, 1.f);
        const float x = s.startX + (s.endX - s.startX) * t;
        const float y = s.startY + (s.endY - s.startY) * t;
        // The block of 4 x 4 tiles holding the point, grown by a tile as the terrain pick does.
        const float blockX = static_cast<float>(static_cast<int>(x / Tile) / 4 * 4);
        const float blockY = static_cast<float>(static_cast<int>(y / Tile) / 4 * 4);
        CHECK(SegmentCrossesBox(s, (blockX - 1.f) * Tile, (blockY - 1.f) * Tile, (blockX + 5.f) * Tile,
                                (blockY + 5.f) * Tile));
    }
}
