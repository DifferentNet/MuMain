#include <doctest.h>

#include "Render/Effects/GroundMarks.h"

#include <set>

namespace
{
namespace GroundMarks = Render::Effects::GroundMarks;
using GroundMarks::Kind;
using GroundMarks::Mark;
using GroundMarks::MarkPool;

constexpr Kind kAllKinds[] = {Kind::Scorch, Kind::LargeScorch, Kind::Frost, Kind::Crack};

Mark MarkAt(const float x, const float y, const Kind kind = Kind::Scorch, const float half = 80.f)
{
    Mark mark;
    mark.x = x;
    mark.y = y;
    mark.halfW = half;
    mark.halfH = half;
    mark.kind = kind;
    return mark;
}
} // namespace

TEST_CASE("a mark fades in, holds and fades out to nothing [render][ground_marks]")
{
    for (const Kind kind : kAllKinds)
    {
        const GroundMarks::Life life = GroundMarks::LifeOf(kind);
        CHECK(GroundMarks::Alpha(0.f, kind) == 0.f);
        CHECK(GroundMarks::Alpha(-1.f, kind) == 0.f);
        CHECK(GroundMarks::Alpha(GroundMarks::kFadeInTicks, kind) == doctest::Approx(life.peak));
        CHECK(GroundMarks::Alpha(life.hold, kind) == doctest::Approx(life.peak));
        CHECK(GroundMarks::Alpha(GroundMarks::Lifetime(kind), kind) == 0.f);
        CHECK(GroundMarks::Alpha(GroundMarks::Lifetime(kind) + 10.f, kind) == 0.f);
        // Close to the end it is almost gone: no pop.
        CHECK(GroundMarks::Alpha(GroundMarks::Lifetime(kind) - 0.01f, kind) < 0.01f);

        // Up to the peak, then never up again.
        float previous = 0.f;
        bool fading = false;
        for (float age = 0.f; age <= GroundMarks::Lifetime(kind) + 1.f; age += 0.25f)
        {
            const float alpha = GroundMarks::Alpha(age, kind);
            CHECK(alpha >= 0.f);
            CHECK(alpha <= life.peak + 1e-6f);
            if (fading)
            {
                CHECK(alpha <= previous + 1e-6f);
            }
            else if (alpha < previous)
            {
                fading = true;
            }
            previous = alpha;
        }
    }
}

TEST_CASE("the lifetimes and peaks of the plan [render][ground_marks]")
{
    CHECK(GroundMarks::Lifetime(Kind::Scorch) == 125.f);
    CHECK(GroundMarks::Lifetime(Kind::LargeScorch) == 125.f);
    CHECK(GroundMarks::Lifetime(Kind::Crack) == 95.f);
    CHECK(GroundMarks::Lifetime(Kind::Frost) == 75.f);
    CHECK(GroundMarks::LifeOf(Kind::Scorch).peak == doctest::Approx(0.6f));
    CHECK(GroundMarks::LifeOf(Kind::LargeScorch).peak == doctest::Approx(0.45f));
    CHECK(GroundMarks::LifeOf(Kind::Crack).peak == doctest::Approx(1.f));
    CHECK(GroundMarks::LifeOf(Kind::Frost).peak == doctest::Approx(0.35f));
    CHECK(GroundMarks::LookOf(GroundMarks::Source::Inferno).kind == Kind::LargeScorch);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::Hellfire).kind == Kind::LargeScorch);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::Meteor).kind == Kind::Scorch);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::Flame).kind == Kind::Scorch);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::IceStorm).kind == Kind::Frost);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::Ice).kind == Kind::Frost);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::RagefulBlow).kind == Kind::Crack);
    // The crack art is wide; the storm's one frost covers its 300 x 300 area.
    CHECK(GroundMarks::LookOf(GroundMarks::Source::RagefulBlow).halfW == 85.f);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::RagefulBlow).halfH == 50.f);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::IceStorm).halfW == GroundMarks::kMaxHalfSize);
    CHECK(GroundMarks::LookOf(GroundMarks::Source::IceStorm).halfH == GroundMarks::kMaxHalfSize);
    CHECK(GroundMarks::MergeShareOf(Kind::Scorch) == 1.f);
    CHECK(GroundMarks::MergeShareOf(Kind::LargeScorch) == 1.f);
    CHECK(GroundMarks::MergeShareOf(Kind::Frost) == doctest::Approx(0.6f));
    CHECK(GroundMarks::MergeShareOf(Kind::Crack) == doctest::Approx(0.6f));
}

TEST_CASE("a hit close to a mark of the same kind renews it [render][ground_marks]")
{
    MarkPool pool;
    REQUIRE(pool.Add(MarkAt(1000.f, 1000.f), 48));
    pool.Update(40.f, 48);
    REQUIRE(pool.At(0).age == doctest::Approx(40.f));

    // A scorch reaches its whole half size: 30 units away renews, back to the start of the hold.
    CHECK(pool.Add(MarkAt(1030.f, 1000.f), 48));
    CHECK(pool.Count() == 1);
    CHECK(pool.At(0).age == doctest::Approx(GroundMarks::kFadeInTicks));
    CHECK(pool.At(0).x == 1000.f);

    // Still fading in: it goes on fading in.
    pool.Update(-pool.At(0).age + 0.5f, 48);
    CHECK(pool.Add(MarkAt(1000.f, 1010.f), 48));
    CHECK(pool.At(0).age == doctest::Approx(0.5f));

    // A wider hit widens it.
    CHECK(pool.Add(MarkAt(1000.f, 1000.f, Kind::Scorch, 120.f), 48));
    CHECK(pool.Count() == 1);
    CHECK(pool.At(0).halfW == 120.f);

    // Farther away than 120, or another kind: a mark of its own.
    CHECK(pool.Add(MarkAt(1130.f, 1000.f), 48));
    CHECK(pool.Add(MarkAt(1000.f, 1000.f, Kind::Frost), 48));
    CHECK(pool.Count() == 3);

    // A frost reaches 0.6 x 80 = 48: 40 away renews, 60 away does not.
    CHECK(pool.Add(MarkAt(1040.f, 1000.f, Kind::Frost), 48));
    CHECK(pool.Count() == 3);
    CHECK(pool.Add(MarkAt(1000.f, 1060.f, Kind::Frost), 48));
    CHECK(pool.Count() == 4);
}

TEST_CASE("a full pool gives up its oldest mark first [render][ground_marks]")
{
    MarkPool pool;
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(pool.Add(MarkAt(1000.f + 500.f * i, 1000.f), 3));
        pool.Update(1.f, 3);
    }
    // Ages 3, 2, 1: the first goes.
    CHECK(pool.Add(MarkAt(5000.f, 5000.f), 3));
    REQUIRE(pool.Count() == 3);
    CHECK(pool.At(0).x == 1500.f);
    CHECK(pool.At(1).x == 2000.f);
    CHECK(pool.At(2).x == 5000.f);
    CHECK(pool.At(2).age == 0.f);

    // A renewed mark is young again: the next oldest goes.
    pool.Update(10.f, 3);
    CHECK(pool.Add(MarkAt(1500.f, 1000.f), 3));
    CHECK(pool.Add(MarkAt(7000.f, 7000.f), 3));
    REQUIRE(pool.Count() == 3);
    CHECK(pool.At(0).x == 1500.f);
    CHECK(pool.At(1).x == 5000.f);
    CHECK(pool.At(2).x == 7000.f);

    // A lower budget (effect level down) drops the oldest; budget 0 drops all and lays nothing.
    pool.Update(0.f, 1);
    REQUIRE(pool.Count() == 1);
    CHECK(pool.At(0).x == 7000.f);
    CHECK_FALSE(pool.Add(MarkAt(100.f, 100.f), 0));
    pool.Update(0.f, 0);
    CHECK(pool.Count() == 0);
}

TEST_CASE("a full pool gives up the mark furthest through its life [render][ground_marks]")
{
    MarkPool pool;
    REQUIRE(pool.Add(MarkAt(1000.f, 1000.f, Kind::Scorch), 2));
    pool.Update(10.f, 2);
    REQUIRE(pool.Add(MarkAt(3000.f, 3000.f, Kind::Frost), 2));
    pool.Update(20.f, 2);
    // Scorch 30 of 125, frost 20 of 75: the frost is further through its life although younger.
    REQUIRE(pool.At(0).age > pool.At(1).age);
    CHECK(pool.Add(MarkAt(5000.f, 5000.f), 2));
    REQUIRE(pool.Count() == 2);
    CHECK(pool.At(0).kind == Kind::Scorch);
    CHECK(pool.At(1).x == 5000.f);
}

TEST_CASE("the pool never holds more than 48 and drops faded marks [render][ground_marks]")
{
    MarkPool pool;
    for (int i = 0; i < 200; ++i)
    {
        pool.Add(MarkAt(200.f * static_cast<float>(i % 60), 200.f * static_cast<float>(i / 60)), 1000);
    }
    CHECK(pool.Count() == GroundMarks::kPoolSize);
    pool.Update(GroundMarks::Lifetime(Kind::Scorch), 1000);
    CHECK(pool.Count() == 0);

    // Too wide a mark is held to the widest.
    pool.Add(MarkAt(0.f, 0.f, Kind::Scorch, 1000.f), 48);
    CHECK(pool.At(0).halfW == GroundMarks::kMaxHalfSize);
}

TEST_CASE("the tiles under a mark and its pieces [render][ground_marks]")
{
    // 1050 +- 80: 970..1130, tiles 9..11.
    const GroundMarks::TileSpan span = GroundMarks::TilesUnder(1050.f, 80.f);
    CHECK(span.first == 9);
    CHECK(span.last == 11);
    // Right on the borders: 1000..1100 is tile 10 only.
    const GroundMarks::TileSpan exact = GroundMarks::TilesUnder(1050.f, 50.f);
    CHECK(exact.first == 10);
    CHECK(exact.last == 10);
    // The widest mark covers at most 4 tiles on each axis.
    for (float center = 1000.f; center < 1100.f; center += 7.f)
    {
        const GroundMarks::TileSpan wide = GroundMarks::TilesUnder(center, GroundMarks::kMaxHalfSize);
        CHECK(wide.last - wide.first + 1 <= GroundMarks::kMaxTilesAcross);
    }

    const GroundMarks::Span first = GroundMarks::ClipToTile(1050.f, 80.f, 9);
    CHECK(first.lo == 970.f);
    CHECK(first.hi == 1000.f);
    const GroundMarks::Span middle = GroundMarks::ClipToTile(1050.f, 80.f, 10);
    CHECK(middle.lo == 1000.f);
    CHECK(middle.hi == 1100.f);
    const GroundMarks::Span last = GroundMarks::ClipToTile(1050.f, 80.f, 11);
    CHECK(last.lo == 1100.f);
    CHECK(last.hi == 1130.f);
    const GroundMarks::Span outside = GroundMarks::ClipToTile(1050.f, 80.f, 12);
    CHECK(outside.hi <= outside.lo);
    CHECK(GroundMarks::FloorToInt(-0.5f) == -1);
    CHECK(GroundMarks::FloorToInt(2.f) == 2);
}

TEST_CASE("the mirror variant is the same for the same spot [render][ground_marks]")
{
    CHECK(GroundMarks::MirrorOf(12345.f, 6789.f) == GroundMarks::MirrorOf(12345.f, 6789.f));
    std::set<int> variants;
    for (int i = 0; i < 64; ++i)
    {
        const std::uint8_t mirror = GroundMarks::MirrorOf(100.f * i, 37.f * i);
        CHECK(mirror < 8);
        variants.insert(mirror);
    }
    // Spread over the variants.
    CHECK(variants.size() >= 6);

    const GroundMarks::UV plain = GroundMarks::MirroredUV(0.25f, 0.75f, 0);
    CHECK(plain.u == 0.25f);
    CHECK(plain.v == 0.75f);
    const GroundMarks::UV flipped = GroundMarks::MirroredUV(0.25f, 0.75f, 3);
    CHECK(flipped.u == 0.75f);
    CHECK(flipped.v == 0.25f);
    const GroundMarks::UV swapped = GroundMarks::MirroredUV(0.25f, 0.75f, 4);
    CHECK(swapped.u == 0.75f);
    CHECK(swapped.v == 0.25f);
}

TEST_CASE("a mark that is not square never swaps U and V [render][ground_marks]")
{
    bool squareSwaps = false;
    for (int i = 0; i < 64; ++i)
    {
        const float x = 100.f * i;
        const float y = 37.f * i;
        const std::uint8_t wide = GroundMarks::MirrorFor(x, y, 85.f, 50.f);
        CHECK((wide & 4u) == 0);
        CHECK(wide == (GroundMarks::MirrorOf(x, y) & 3u));
        const std::uint8_t square = GroundMarks::MirrorFor(x, y, 80.f, 80.f);
        CHECK(square == GroundMarks::MirrorOf(x, y));
        squareSwaps = squareSwaps || (square & 4u) != 0;
    }
    CHECK(squareSwaps);
    // So the wide art stays wide: U runs along x across the 170, V along y across the 100.
    for (std::uint8_t mirror = 0; mirror < 4; ++mirror)
    {
        const GroundMarks::UV corner = GroundMarks::MirroredUV(1.f, 0.f, mirror);
        CHECK((corner.u == 0.f || corner.u == 1.f));
        CHECK(((mirror & 1u) != 0 ? corner.u == 0.f : corner.u == 1.f));
    }
}
