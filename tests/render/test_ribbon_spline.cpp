#include <doctest.h>

#include "Render/Effects/RibbonSpline.h"

#include <array>
#include <cmath>

namespace
{
namespace Spline = Render::Effects::RibbonSpline;
using doctest::Approx;

constexpr float kHalfWidth = 20.0f;

// A tail around `centre` the way SetNewestTail lays it unrotated: corners 0/1 along x, 2/3 along z.
void SetTail(Spline::Tail& tail, const float x, const float y, const float z)
{
    const float centre[3] = {x, y, z};
    for (int corner = 0; corner < Spline::kCorners; ++corner)
    {
        for (int axis = 0; axis < Spline::kAxes; ++axis)
        {
            tail[corner][axis] = centre[axis];
        }
    }
    tail[0][0] -= kHalfWidth;
    tail[1][0] += kHalfWidth;
    tail[2][2] -= kHalfWidth;
    tail[3][2] += kHalfWidth;
}

struct Trail
{
    Spline::Tail tails[Spline::kMaxRibbonTails + 1]{};
};

// Tails along y, `spacing` apart, away from the origin.
Trail StraightTrail(const int count, const float spacing)
{
    Trail trail;
    for (int j = 0; j < count; ++j)
    {
        SetTail(trail.tails[j], 100.0f, 100.0f + spacing * static_cast<float>(j), 50.0f);
    }
    return trail;
}

// A trail that turns 40 degrees at every tail, as an Evil Spirit spirit weaving.
Trail CurvedTrail(const int count, const float spacing)
{
    Trail trail;
    float x = 100.0f;
    float y = 100.0f;
    float heading = 0.0f;
    constexpr float kTurn = 0.6981317f; // 40 degrees
    for (int j = 0; j < count; ++j)
    {
        SetTail(trail.tails[j], x, y, 50.0f);
        x += spacing * std::cos(heading);
        y += spacing * std::sin(heading);
        heading += kTurn;
    }
    return trail;
}

float Centre(const Spline::RibbonPoint& point, const int axis)
{
    return (point.corner[0][axis] + point.corner[1][axis]) * 0.5f;
}

bool AllFinite(const Spline::RibbonPoint* points, const int count)
{
    for (int i = 0; i < count; ++i)
    {
        for (int corner = 0; corner < Spline::kCorners; ++corner)
        {
            for (int axis = 0; axis < Spline::kAxes; ++axis)
            {
                if (!std::isfinite(points[i].corner[corner][axis]))
                {
                    return false;
                }
            }
        }
        if (!std::isfinite(points[i].u))
        {
            return false;
        }
    }
    return true;
}

// The span p1 -> p2 never strays from its box by more than `slack` of its chord.
void CheckStaysNearSpan(const float* p0, const float* p1, const float* p2, const float* p3, const float slack)
{
    const float d01 = Spline::Distance(p0, p1);
    const float d12 = Spline::Distance(p1, p2);
    const float d23 = Spline::Distance(p2, p3);
    const float margin = slack * d12;
    for (int step = 0; step <= 100; ++step)
    {
        float out[3];
        Spline::CatmullRom(p0, p1, p2, p3, d01, d12, d23, static_cast<float>(step) / 100.0f, out);
        for (int axis = 0; axis < 3; ++axis)
        {
            CHECK(out[axis] >= std::min(p1[axis], p2[axis]) - margin);
            CHECK(out[axis] <= std::max(p1[axis], p2[axis]) + margin);
        }
    }
}

std::array<Spline::RibbonPoint, 300> s_points;
} // namespace

TEST_CASE("the curve starts and ends exactly on its tails [render][ribbon_spline]")
{
    const float p0[3] = {0.0f, 0.0f, 0.0f};
    const float p1[3] = {13.7f, 4.1f, -2.0f};
    const float p2[3] = {31.3f, -9.9f, 7.5f};
    const float p3[3] = {40.0f, 3.0f, 1.0f};
    const float d01 = Spline::Distance(p0, p1);
    const float d12 = Spline::Distance(p1, p2);
    const float d23 = Spline::Distance(p2, p3);
    float start[3];
    float end[3];
    Spline::CatmullRom(p0, p1, p2, p3, d01, d12, d23, 0.0f, start);
    Spline::CatmullRom(p0, p1, p2, p3, d01, d12, d23, 1.0f, end);
    for (int axis = 0; axis < 3; ++axis)
    {
        CHECK(start[axis] == p1[axis]);
        CHECK(end[axis] == p2[axis]);
    }
}

TEST_CASE("points on a line stay on it, in order [render][ribbon_spline]")
{
    const float p0[3] = {0.0f, 0.0f, 0.0f};
    const float p1[3] = {5.0f, 0.0f, 0.0f};
    const float p2[3] = {50.0f, 0.0f, 0.0f};
    const float p3[3] = {52.0f, 0.0f, 0.0f};
    float previous = p1[0];
    for (int step = 1; step <= 20; ++step)
    {
        float out[3];
        Spline::CatmullRom(p0, p1, p2, p3, 5.0f, 45.0f, 2.0f, static_cast<float>(step) / 20.0f, out);
        CHECK(out[1] == Approx(0.0f));
        CHECK(out[2] == Approx(0.0f));
        CHECK(out[0] >= previous);
        CHECK(out[0] <= p2[0] + 0.001f);
        previous = out[0];
    }
}

TEST_CASE("a short span between long ones and a hairpin do not overshoot [render][ribbon_spline]")
{
    // A uniform Catmull-Rom loops out here; the centripetal one stays close to the span.
    const float a0[3] = {0.0f, 0.0f, 0.0f};
    const float a1[3] = {100.0f, 0.0f, 0.0f};
    const float a2[3] = {101.0f, 0.0f, 0.0f};
    const float a3[3] = {101.0f, 100.0f, 0.0f};
    CheckStaysNearSpan(a0, a1, a2, a3, 0.25f);

    const float b0[3] = {0.0f, 0.0f, 0.0f};
    const float b1[3] = {100.0f, 0.0f, 0.0f};
    const float b2[3] = {100.0f, 10.0f, 0.0f};
    const float b3[3] = {0.0f, 10.0f, 0.0f};
    CheckStaysNearSpan(b0, b1, b2, b3, 0.25f);
}

TEST_CASE("coincident tails make no NaN [render][ribbon_spline]")
{
    // Death Stab's wind: its last tails sit on one spot, and the first two may too.
    Trail trail = StraightTrail(5, 30.0f);
    SetTail(trail.tails[1], 100.0f, 100.0f, 50.0f);
    SetTail(trail.tails[3], 100.0f, 160.0f, 50.0f);
    SetTail(trail.tails[4], 100.0f, 160.0f, 50.0f);
    for (const float phase : {0.2f, 1.0f})
    {
        const int count = Spline::Build(trail.tails, 5, 6, phase, 0.2f, 8, s_points.data(), 300);
        CHECK(count >= 2);
        CHECK(AllFinite(s_points.data(), count));
    }

    Trail still = StraightTrail(4, 0.0f);
    const int count = Spline::Build(still.tails, 4, 5, 1.0f, 1.0f, 8, s_points.data(), 300);
    CHECK(count == 4);
    CHECK(AllFinite(s_points.data(), count));
}

TEST_CASE("a span takes one piece per 5 degrees of turn, within the budget [render][ribbon_spline]")
{
    CHECK(Spline::Pieces(0.0f, 8) == 1);
    CHECK(Spline::Pieces(5.0f, 8) == 1);
    CHECK(Spline::Pieces(5.01f, 8) == 2);
    CHECK(Spline::Pieces(12.0f, 8) == 3);
    CHECK(Spline::Pieces(100.0f, 8) == 8);
    CHECK(Spline::Pieces(100.0f, 4) == 4);
    CHECK(Spline::Pieces(100.0f, 1) == 1);
    CHECK(Spline::Pieces(100.0f, 0) == 1);
}

TEST_CASE("tail ages and U match the classic trail at 25 FPS [render][ribbon_spline]")
{
    for (int slot = 0; slot < 10; ++slot)
    {
        CHECK(Spline::TailAge(slot, 1.0f) == Approx(static_cast<float>(slot)));
    }
    // A full trail: NumTails = MaxTails - 1, the slots 0..NumTails laid.
    for (const int maxTails : {5, 6, 30})
    {
        const int numTails = maxTails - 1;
        const float endAge = Spline::EndAge(numTails, 1.0f, maxTails, 1.0f);
        CHECK(endAge == Approx(static_cast<float>(numTails)));
        for (int j = 0; j <= numTails; ++j)
        {
            const float classic = static_cast<float>(numTails - j) / static_cast<float>(maxTails - 1);
            CHECK(Spline::TrailU(Spline::TailAge(j, 1.0f), endAge, maxTails, 1.0f) == Approx(classic));
        }
    }
}

TEST_CASE("U moves by one frame's share each frame, not one tail a tick [render][ribbon_spline]")
{
    constexpr int kMaxTails = 6;
    constexpr float kFactor = 0.25f; // 100 FPS
    const float fullLength = Spline::FullLength(kMaxTails, kFactor);
    const float endAge = Spline::EndAge(kMaxTails - 1, 0.5f, kMaxTails, kFactor);
    CHECK(endAge == Approx(fullLength));
    for (int slot = 1; slot < kMaxTails; ++slot)
    {
        const float before = Spline::TrailU(Spline::TailAge(slot, 0.5f), endAge, kMaxTails, kFactor);
        const float after = Spline::TrailU(Spline::TailAge(slot, 0.5f + kFactor), endAge, kMaxTails, kFactor);
        CHECK(before - after == Approx(kFactor / fullLength));
    }
}

TEST_CASE("the trail ends at its oldest tail while growing and is cut once full [render][ribbon_spline]")
{
    // Growing: four tails laid of six.
    CHECK(Spline::EndAge(3, 0.5f, 6, 0.25f) == Approx(2.5f));
    // Full, just after a tail was laid: the oldest tail is the end.
    CHECK(Spline::EndAge(5, 0.25f, 6, 0.25f) == Approx(4.25f));
    // Full, later in the tick: the oldest tail is past the length, so the end is cut.
    CHECK(Spline::EndAge(5, 0.75f, 6, 0.25f) == Approx(4.25f));
}

TEST_CASE("a straight trail is one piece per span [render][ribbon_spline]")
{
    const Trail trail = StraightTrail(6, 40.0f);
    CHECK(Spline::Build(trail.tails, 6, 6, 1.0f, 1.0f, 8, s_points.data(), 300) == 6);
    // At 100 FPS just after a tail was laid nothing is cut either.
    CHECK(Spline::Build(trail.tails, 6, 6, 0.25f, 0.25f, 8, s_points.data(), 300) == 6);
}

TEST_CASE("one piece a span at 25 FPS is the classic trail [render][ribbon_spline]")
{
    constexpr int kMaxTails = 6;
    const Trail trail = CurvedTrail(kMaxTails, 70.0f);
    const int count = Spline::Build(trail.tails, kMaxTails, kMaxTails, 1.0f, 1.0f, 1, s_points.data(), 300);
    REQUIRE(count == kMaxTails);
    for (int j = 0; j < count; ++j)
    {
        for (int corner = 0; corner < Spline::kCorners; ++corner)
        {
            for (int axis = 0; axis < Spline::kAxes; ++axis)
            {
                CHECK(s_points[j].corner[corner][axis] == trail.tails[j][corner][axis]);
            }
        }
        const float classic = static_cast<float>(kMaxTails - 1 - j) / static_cast<float>(kMaxTails - 1);
        CHECK(s_points[j].u == Approx(classic));
    }
}

TEST_CASE("a curved trail gets pieces that pass through its tails [render][ribbon_spline]")
{
    constexpr int kMaxTails = 6;
    const Trail trail = CurvedTrail(kMaxTails, 70.0f);
    const int count = Spline::Build(trail.tails, kMaxTails, kMaxTails, 1.0f, 1.0f, 8, s_points.data(), 300);
    // 40 degrees a tail: 8 pieces for each of the 5 spans, plus the head.
    CHECK(count == 1 + 5 * 8);
    CHECK(AllFinite(s_points.data(), count));
    for (int j = 0; j < kMaxTails; ++j)
    {
        for (int axis = 0; axis < Spline::kAxes; ++axis)
        {
            const float tailCentre = (trail.tails[j][0][axis] + trail.tails[j][1][axis]) * 0.5f;
            CHECK(Centre(s_points[j * 8], axis) == Approx(tailCentre));
        }
    }
    // U falls from 1 at the head to 0 at the end.
    CHECK(s_points[0].u == Approx(1.0f));
    CHECK(s_points[count - 1].u == Approx(0.0f));
    for (int i = 1; i < count; ++i)
    {
        CHECK(s_points[i].u < s_points[i - 1].u);
    }
}

TEST_CASE("the last span is cut where the trail ends [render][ribbon_spline]")
{
    // 100 FPS, three quarters of a tick after the last tail: ages 0, .75, 1.75 .. 4.75, cut at 4.25.
    const Trail trail = StraightTrail(6, 10.0f);
    const int count = Spline::Build(trail.tails, 6, 6, 0.75f, 0.25f, 8, s_points.data(), 300);
    REQUIRE(count == 6);
    CHECK(Centre(s_points[count - 1], 1) == Approx(100.0f + 45.0f));
    CHECK(s_points[count - 1].u == Approx(0.0f));
}

TEST_CASE("a growing trail runs from its head to its oldest tail [render][ribbon_spline]")
{
    // Three tails of six laid, at 100 FPS half a tick after the last one: ages 0, .5, 1.5.
    constexpr int kMaxTails = 6;
    constexpr float kFactor = 0.25f;
    const Trail trail = StraightTrail(3, 40.0f);
    const int count = Spline::Build(trail.tails, 3, kMaxTails, 0.5f, kFactor, 8, s_points.data(), 300);
    REQUIRE(count == 3);
    CHECK(s_points[0].u == Approx(1.5f / 4.25f));
    CHECK(s_points[count - 1].u == Approx(0.0f));
    CHECK(Centre(s_points[count - 1], 1) == Approx(100.0f + 80.0f));
}

TEST_CASE("two tails make one straight span with both ends exact [render][ribbon_spline]")
{
    const Trail trail = CurvedTrail(2, 70.0f);
    const int count = Spline::Build(trail.tails, 2, 6, 0.5f, 0.25f, 8, s_points.data(), 300);
    REQUIRE(count == 2);
    CHECK(AllFinite(s_points.data(), count));
    for (int j = 0; j < count; ++j)
    {
        for (int corner = 0; corner < Spline::kCorners; ++corner)
        {
            for (int axis = 0; axis < Spline::kAxes; ++axis)
            {
                CHECK(s_points[j].corner[corner][axis] == trail.tails[j][corner][axis]);
            }
        }
    }
    CHECK(s_points[0].u == Approx(0.5f / Spline::FullLength(6, 0.25f)));
    CHECK(s_points[1].u == Approx(0.0f));

    // Two tails on one spot: the linear span, its ends exact too.
    const Trail still = StraightTrail(2, 0.0f);
    REQUIRE(Spline::Build(still.tails, 2, 6, 1.0f, 1.0f, 8, s_points.data(), 300) == 2);
    CHECK(s_points[1].corner[3][2] == still.tails[1][3][2]);
}

TEST_CASE("the point cap and the tail limits hold [render][ribbon_spline]")
{
    const Trail trail = CurvedTrail(6, 70.0f);
    CHECK(Spline::Build(trail.tails, 6, 6, 1.0f, 1.0f, 8, s_points.data(), 3) == 3);
    CHECK(Spline::Build(trail.tails, 6, 6, 1.0f, 1.0f, 8, s_points.data(), 0) == 0);
    CHECK(Spline::Build(trail.tails, 1, 6, 1.0f, 1.0f, 8, s_points.data(), 300) == 0);
    const Trail longTrail = StraightTrail(Spline::kMaxRibbonTails + 1, 10.0f);
    CHECK(Spline::Build(longTrail.tails, Spline::kMaxRibbonTails + 1, 40, 1.0f, 1.0f, 8, s_points.data(), 300) == 0);
    CHECK(Spline::kMinRibbonTails == 3);
}

TEST_CASE("a new trail takes the most pieces only when its longest fits the budget [render][ribbon_spline]")
{
    // Evil Spirit at level 4: 5 spans x 8 pieces x 2 faces.
    constexpr int worst = 5 * 8 * 2;
    const Spline::StickyChoice fits = Spline::StickyPieces(0, false, 8, 1000, worst, 4096);
    CHECK(fits.kept == 8);
    CHECK(fits.built == 8);
    // Exactly at the budget still fits.
    CHECK(Spline::StickyPieces(0, false, 8, 4096 - worst, worst, 4096).kept == 8);
    // Past it: 1 piece a span, never the classic draw.
    const Spline::StickyChoice over = Spline::StickyPieces(0, false, 8, 4096 - worst + 1, worst, 4096);
    CHECK(over.kept == 1);
    CHECK(over.built == 1);
    // Not drawn the frame before: it chooses again.
    CHECK(Spline::StickyPieces(1, false, 8, 0, worst, 4096).kept == 8);
}

TEST_CASE("a drawn trail keeps its pieces whatever the budget does [render][ribbon_spline]")
{
    constexpr int worst = 5 * 8 * 2;
    // Kept 8 although the frame is now past the budget (below twice it).
    const Spline::StickyChoice keeps = Spline::StickyPieces(8, true, 8, 5000, worst, 4096);
    CHECK(keeps.kept == 8);
    CHECK(keeps.built == 8);
    // Kept 1 although the frame has room again.
    const Spline::StickyChoice stays = Spline::StickyPieces(1, true, 8, 0, worst, 4096);
    CHECK(stays.kept == 1);
    CHECK(stays.built == 1);
    // The level dropped (4 -> 3): the kept choice is clamped to the new most.
    CHECK(Spline::StickyPieces(8, true, 4, 0, 5 * 4 * 2, 2048).kept == 4);
}

TEST_CASE("past twice the budget a trail is built with 1 piece that frame only [render][ribbon_spline]")
{
    constexpr int worst = 5 * 8 * 2;
    const Spline::StickyChoice capped = Spline::StickyPieces(8, true, 8, 2 * 4096 - worst + 1, worst, 4096);
    CHECK(capped.kept == 8);
    CHECK(capped.built == 1);
    // Right at twice the budget it is still built with its pieces.
    CHECK(Spline::StickyPieces(8, true, 8, 2 * 4096 - worst, worst, 4096).built == 8);
    // The cap counts the trail's longest at its kept pieces: 4 of 8 halves it.
    CHECK(Spline::StickyPieces(4, true, 8, 2 * 4096 - worst / 2, worst, 4096).built == 4);
    CHECK(Spline::StickyPieces(4, true, 8, 2 * 4096 - worst / 2 + 1, worst, 4096).built == 1);
}
