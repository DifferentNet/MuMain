#include "doctest.h"

#include "UI/HUD/LiveGaugeMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace UI::HUD::LiveGauge;

namespace
{
// Skynet's gem rect in its ring (main_frame_live.rcss) at 1 px per dp.
constexpr Box GemBox{4.f, 7.6f, 54.f, 46.8f};
constexpr float Zoom = 1.15f;
constexpr float Near = 1e-3f;

constexpr std::uint32_t EmptyGrey = 0xFF4D4D4Du; // 0.30
constexpr std::uint32_t GhostGrey = 0xFF999999u; // 0.60
constexpr std::uint32_t FillGrey = 0xFFFFFFFFu;  // 1

GemShape Shape(const Box& box = GemBox)
{
    GemShape shape;
    const std::vector<OutlinePoint> outline = DefaultOutline();
    ShapeGem(box, outline, Zoom, shape);
    return shape;
}

std::size_t ColumnOf(const GemShape& shape, float x)
{
    for (std::size_t i = 0; i < shape.xs.size(); ++i)
    {
        if (std::fabs(shape.xs[i] - x) <= Near)
            return i;
    }
    FAIL("a vertex off the shape's column edges");
    return 0;
}

void CheckQuadsInside(const std::vector<Vertex>& quads, const Box& box, float margin = Near)
{
    CHECK(quads.size() % 4 == 0);
    const float uLow = 0.5f - 0.5f / Zoom - Near;
    const float uHigh = 0.5f + 0.5f / Zoom + Near;
    for (const Vertex& vertex : quads)
    {
        CHECK(vertex.x >= box.left - margin);
        CHECK(vertex.x <= box.left + box.width + margin);
        CHECK(vertex.y >= box.top - margin);
        CHECK(vertex.y <= box.top + box.height + margin);
        CHECK(vertex.u >= uLow);
        CHECK(vertex.u <= uHigh);
        CHECK(vertex.v >= uLow);
        CHECK(vertex.v <= uHigh);
    }
}

std::size_t CountColoured(const std::vector<Vertex>& quads, std::uint32_t colour)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < quads.size(); i += 4)
        count += quads[i].abgr == colour ? 1 : 0;
    return count;
}

std::uint32_t Channel(std::uint32_t abgr)
{
    return abgr & 0xFFu;
}

// Skynet's SD ring at 1 px per dp: the value band inside the 27.5..31 track, round the gem's centre.
constexpr Ring ValueRing{31.f, 31.f, 28.f, 30.5f, 140.f, 260.f};
constexpr float DegreesPerRadian = 57.29577951308232f;
constexpr float AngleNear = 1e-2f;

float RadiusOf(const Ring& ring, const Vertex& vertex)
{
    return std::hypot(vertex.x - ring.centreX, vertex.y - ring.centreY);
}

// A vertex's angle round the ring's centre, in degrees past its start: 0..360.
float SweptTo(const Ring& ring, const Vertex& vertex)
{
    float degrees = std::atan2(vertex.y - ring.centreY, vertex.x - ring.centreX) * DegreesPerRadian - ring.startDegrees;
    while (degrees < -AngleNear)
        degrees += 360.f;
    while (degrees >= 360.f - AngleNear)
        degrees -= 360.f;
    return degrees;
}

// Every vertex in the band and within `fromDegrees`..`toDegrees` past the start; u at the band's two edges, v along
// the sweep.
void CheckArcVertices(const std::vector<Vertex>& quads, const Ring& ring, float fromDegrees, float toDegrees)
{
    CHECK(quads.size() % 4 == 0);
    for (const Vertex& vertex : quads)
    {
        const float radius = RadiusOf(ring, vertex);
        CHECK(radius >= ring.inner - Near);
        CHECK(radius <= ring.outer + Near);
        const float swept = SweptTo(ring, vertex);
        CHECK(swept >= fromDegrees - AngleNear);
        CHECK(swept <= toDegrees + AngleNear);
        if (std::fabs(radius - ring.outer) <= Near)
            CHECK(vertex.u == doctest::Approx(0.15f));
        else
            CHECK(vertex.u == doctest::Approx(0.85f));
        CHECK(vertex.v == doctest::Approx(0.05f + 0.9f * swept / ring.sweepDegrees).epsilon(1e-3));
    }
}
} // namespace

TEST_CASE("a gem's silhouette is sampled at even columns and its corners [ui][live_gauge]")
{
    const GemShape shape = Shape();
    REQUIRE(shape.xs.size() >= 17);
    REQUIRE(shape.tops.size() == shape.xs.size());
    REQUIRE(shape.bottoms.size() == shape.xs.size());
    CHECK(shape.xs.front() == doctest::Approx(GemBox.left + 0.05f * GemBox.width));
    CHECK(shape.xs.back() == doctest::Approx(GemBox.left + 0.96f * GemBox.width));
    CHECK(shape.topY == doctest::Approx(GemBox.top + 0.03f * GemBox.height));
    CHECK(shape.bottomY == doctest::Approx(GemBox.top + 0.97f * GemBox.height));
    for (std::size_t i = 0; i < shape.xs.size(); ++i)
    {
        CAPTURE(i);
        if (i > 0)
            CHECK(shape.xs[i] > shape.xs[i - 1]);
        CHECK(shape.tops[i] >= shape.topY - Near);
        CHECK(shape.bottoms[i] <= shape.bottomY + Near);
        CHECK(shape.tops[i] < shape.bottoms[i]);
    }
    // The octagon's vertical sides reach both their corners.
    CHECK(shape.tops.front() == doctest::Approx(GemBox.top + 0.32f * GemBox.height));
    CHECK(shape.bottoms.front() == doctest::Approx(GemBox.top + 0.70f * GemBox.height));
    // Mid-width: the flat top and bottom.
    const std::size_t middle = ColumnOf(shape, GemBox.left + 0.5f * (0.05f + 0.96f) * GemBox.width);
    CHECK(shape.tops[middle] == doctest::Approx(shape.topY));
    CHECK(shape.bottoms[middle] == doctest::Approx(shape.bottomY));

    CHECK(shape.LevelY(0.f) == doctest::Approx(shape.bottomY));
    CHECK(shape.LevelY(1.f) == doctest::Approx(shape.topY));
    CHECK(shape.LevelY(0.5f) == doctest::Approx(0.5f * (shape.topY + shape.bottomY)));
    CHECK(shape.LevelY(-1.f) == doctest::Approx(shape.bottomY));
    CHECK(shape.LevelY(2.f) == doctest::Approx(shape.topY));
    CHECK(shape.U(GemBox.left) == doctest::Approx(0.5f - 0.5f / Zoom));
    CHECK(shape.U(GemBox.left + GemBox.width) == doctest::Approx(0.5f + 0.5f / Zoom));
    CHECK(shape.V(GemBox.top + 0.5f * GemBox.height) == doctest::Approx(0.5f));

    GemShape empty;
    ShapeGem(Box{0.f, 0.f, 0.f, 10.f}, DefaultOutline(), Zoom, empty);
    CHECK(empty.xs.empty());
}

TEST_CASE("the liquid's surface is its level's line plus the wave, inside the silhouette [ui][live_gauge]")
{
    const GemShape shape = Shape();
    std::vector<float> surface;
    ShapeSurface(shape, 0.5f, Wave{0.0, 2.6f, 0.f}, surface);
    REQUIRE(surface.size() == shape.xs.size());
    for (const float y : surface)
        CHECK(y == doctest::Approx(shape.LevelY(0.5f)));

    for (const double time : {0.0, 0.4, 1.7})
    {
        ShapeSurface(shape, 0.5f, Wave{time, 2.6f, 3.f}, surface);
        for (std::size_t i = 0; i < surface.size(); ++i)
        {
            CHECK(std::fabs(surface[i] - shape.LevelY(0.5f)) <= 3.f + Near);
            CHECK(surface[i] >= shape.tops[i]);
            CHECK(surface[i] <= shape.bottoms[i]);
        }
    }

    // Near the top the line leaves the slanted corners: it is kept on the silhouette.
    ShapeSurface(shape, 0.95f, Wave{0.0, 2.6f, 0.f}, surface);
    CHECK(surface.front() == doctest::Approx(shape.tops.front()));
}

TEST_CASE("a gem's body is three stacked parts per column: empty, ghost and liquid [ui][live_gauge]")
{
    const GemShape shape = Shape();
    std::vector<float> surface;
    std::vector<Vertex> quads;

    for (const float level : {0.f, 0.1f, 0.5f, 0.9f, 1.f})
    {
        // No ghost (0), and one above the liquid.
        for (const float ghost : {0.f, std::min(1.f, level + 0.3f)})
        {
            CAPTURE(level);
            CAPTURE(ghost);
            ShapeSurface(shape, level, Wave{0.3, 2.6f, 1.2f}, surface);
            BuildOrbBody(shape, surface, ghost, BodyShades{}, quads);
            CheckQuadsInside(quads, GemBox);

            // The parts of a column meet edge to edge and fill it from the top to the bottom.
            std::vector<float> covered0(shape.xs.size(), 0.f);
            std::vector<float> covered1(shape.xs.size(), 0.f);
            for (std::size_t q = 0; q < quads.size(); q += 4)
            {
                const std::size_t i = ColumnOf(shape, quads[q].x);
                REQUIRE(i + 1 < shape.xs.size());
                CHECK(quads[q + 2].x == doctest::Approx(shape.xs[i + 1]));
                CHECK(quads[q + 1].y >= quads[q].y);
                CHECK(quads[q + 2].y >= quads[q + 3].y);
                covered0[i] += quads[q + 1].y - quads[q].y;
                covered1[i] += quads[q + 2].y - quads[q + 3].y;
                if (quads[q].abgr == FillGrey)
                {
                    // The liquid's top edge is the surface, its bottom the silhouette's.
                    CHECK(quads[q].y == doctest::Approx(surface[i]));
                    CHECK(quads[q + 3].y == doctest::Approx(surface[i + 1]));
                    CHECK(quads[q + 1].y == doctest::Approx(shape.bottoms[i]));
                }
            }
            for (std::size_t i = 0; i + 1 < shape.xs.size(); ++i)
            {
                CAPTURE(i);
                // A part under 0.01 px is left out.
                CHECK(std::fabs(covered0[i] - (shape.bottoms[i] - shape.tops[i])) <= 0.05f);
                CHECK(std::fabs(covered1[i] - (shape.bottoms[i + 1] - shape.tops[i + 1])) <= 0.05f);
            }

            if (level == 0.f)
                CHECK(CountColoured(quads, FillGrey) == 0);
            if (level == 1.f)
                CHECK(CountColoured(quads, EmptyGrey) == 0);
            // The wave's troughs under the level's line show the empty art, not a ghost.
            if (ghost == 0.f)
                CHECK(CountColoured(quads, GhostGrey) == 0);
        }
    }

    // A ghost above the liquid shows between its line and the surface.
    ShapeSurface(shape, 0.4f, Wave{0.0, 2.6f, 0.f}, surface);
    BuildOrbBody(shape, surface, 0.8f, BodyShades{}, quads);
    const std::size_t ghosts = CountColoured(quads, GhostGrey);
    CHECK(ghosts > 0);
    for (std::size_t q = 0; q < quads.size(); q += 4)
    {
        if (quads[q].abgr != GhostGrey)
            continue;
        const std::size_t i = ColumnOf(shape, quads[q].x);
        CHECK(quads[q].y == doctest::Approx(std::clamp(shape.LevelY(0.8f), shape.tops[i], surface[i])));
        CHECK(quads[q + 1].y == doctest::Approx(surface[i]));
    }

    // A ghost under the liquid shows nothing.
    ShapeSurface(shape, 0.4f, Wave{0.0, 2.6f, 0.f}, surface);
    BuildOrbBody(shape, surface, 0.2f, BodyShades{}, quads);
    CHECK(CountColoured(quads, GhostGrey) == 0);

    // A surface that does not match the shape builds nothing.
    surface.pop_back();
    BuildOrbBody(shape, surface, 0.5f, BodyShades{}, quads);
    CHECK(quads.empty());
}

TEST_CASE("a gem's shine is a highlight, a sheen and a crest under the surface [ui][live_gauge]")
{
    const GemShape shape = Shape();
    std::vector<float> surface;
    ShapeSurface(shape, 0.5f, Wave{0.0, 2.6f, 0.f}, surface);
    std::vector<Vertex> quads;

    SUBCASE("nothing to add, nothing built")
    {
        ShineInputs dark;
        dark.highlightPeak = 0.f;
        BuildOrbShine(shape, surface, dark, quads);
        CHECK(quads.empty());
    }

    SUBCASE("the glass highlight alone stays faint, at the gem's top left")
    {
        BuildOrbShine(shape, surface, ShineInputs{}, quads);
        REQUIRE_FALSE(quads.empty());
        CheckQuadsInside(quads, GemBox);
        std::uint32_t brightest = 0;
        float brightestX = 0.f;
        float brightestY = 0.f;
        for (const Vertex& vertex : quads)
        {
            CHECK((vertex.abgr >> 24) == 0xFFu);
            if (Channel(vertex.abgr) > brightest)
            {
                brightest = Channel(vertex.abgr);
                brightestX = (vertex.x - GemBox.left) / GemBox.width;
                brightestY = (vertex.y - GemBox.top) / GemBox.height;
            }
        }
        CHECK(brightest <= 31u);
        CHECK(brightest > 10u);
        CHECK(brightestX < 0.5f);
        CHECK(brightestY < 0.5f);
    }

    SUBCASE("the sheen lights a band that moves across the gem")
    {
        ShineInputs sheen;
        sheen.highlightPeak = 0.f;
        float lastCentre = -1.f;
        for (const float phase : {0.25f, 0.5f, 0.75f})
        {
            CAPTURE(phase);
            sheen.sheenPhase = phase;
            BuildOrbShine(shape, surface, sheen, quads);
            REQUIRE_FALSE(quads.empty());
            CheckQuadsInside(quads, GemBox);
            float weighted = 0.f;
            float weight = 0.f;
            for (const Vertex& vertex : quads)
            {
                const float light = static_cast<float>(Channel(vertex.abgr));
                weighted += light * (vertex.x + 0.6f * vertex.y);
                weight += light;
            }
            REQUIRE(weight > 0.f);
            const float centre = weighted / weight;
            CHECK(centre > lastCentre);
            lastCentre = centre;
        }
    }

    SUBCASE("brightening lights every cell of the grid")
    {
        ShineInputs bright;
        bright.highlightPeak = 0.f;
        bright.brighten = 0.2f;
        BuildOrbShine(shape, surface, bright, quads);
        CHECK(quads.size() == (shape.xs.size() - 1) * 8 * 4);
    }

    SUBCASE("the crest runs from the surface down its height, bright on top")
    {
        ShineInputs crest;
        crest.highlightPeak = 0.f;
        crest.crestHeight = 3.f;
        BuildOrbShine(shape, surface, crest, quads);
        REQUIRE(quads.size() == (shape.xs.size() - 1) * 4);
        for (std::size_t q = 0; q < quads.size(); q += 4)
        {
            const std::size_t i = ColumnOf(shape, quads[q].x);
            CHECK(quads[q].y == doctest::Approx(surface[i]));
            CHECK(quads[q + 1].y == doctest::Approx(std::min(surface[i] + 3.f, shape.bottoms[i])));
            CHECK(quads[q].abgr == 0xFF737373u); // 0.45
            CHECK(quads[q + 1].abgr == 0xFF000000u);
        }

        // At the bottom the crest has no room left.
        ShapeSurface(shape, 0.f, Wave{0.0, 2.6f, 0.f}, surface);
        BuildOrbShine(shape, surface, crest, quads);
        CHECK(quads.empty());
    }
}

TEST_CASE("a gem's socket rim is as wide as asked all round the outline [ui][live_gauge]")
{
    const std::vector<OutlinePoint> outline = DefaultOutline();
    constexpr float Width = 1.5f;
    constexpr RimColours Colours{0xFF2A465Cu, 0xFF07090Bu};
    std::vector<Vertex> quads;
    BuildOrbUnder(GemBox, outline, Width, Colours, quads);
    REQUIRE(quads.size() == outline.size() * 4);
    const Box grown{GemBox.left - 2.f * Width, GemBox.top - 2.f * Width, GemBox.width + 4.f * Width,
                    GemBox.height + 4.f * Width};

    float centreX = 0.f;
    float centreY = 0.f;
    for (const OutlinePoint& point : outline)
    {
        centreX += GemBox.left + point.x * GemBox.width;
        centreY += GemBox.top + point.y * GemBox.height;
    }
    centreX /= static_cast<float>(outline.size());
    centreY /= static_cast<float>(outline.size());

    for (std::size_t q = 0; q < quads.size(); q += 4)
    {
        CAPTURE(q / 4);
        const Vertex& outer0 = quads[q];
        const Vertex& inner0 = quads[q + 1];
        const Vertex& inner1 = quads[q + 2];
        const Vertex& outer1 = quads[q + 3];
        CHECK(outer0.abgr == Colours.outer);
        CHECK(outer1.abgr == Colours.outer);
        CHECK(inner0.abgr == Colours.inner);
        CHECK(inner1.abgr == Colours.inner);
        // The inner edge is the outline's side.
        const OutlinePoint& a = outline[q / 4];
        const OutlinePoint& b = outline[(q / 4 + 1) % outline.size()];
        CHECK(inner0.x == doctest::Approx(GemBox.left + a.x * GemBox.width));
        CHECK(inner0.y == doctest::Approx(GemBox.top + a.y * GemBox.height));
        CHECK(inner1.x == doctest::Approx(GemBox.left + b.x * GemBox.width));
        CHECK(inner1.y == doctest::Approx(GemBox.top + b.y * GemBox.height));
        // The outer corners lie `Width` outside that side, away from the gem's centre.
        const float dx = inner1.x - inner0.x;
        const float dy = inner1.y - inner0.y;
        const float length = std::hypot(dx, dy);
        const float centreSide = (centreX - inner0.x) * dy - (centreY - inner0.y) * dx;
        for (const Vertex* outer : {&outer0, &outer1})
        {
            const float side = (outer->x - inner0.x) * dy - (outer->y - inner0.y) * dx;
            CHECK(std::fabs(side) / length == doctest::Approx(Width).epsilon(1e-3));
            CHECK(side * centreSide < 0.f);
        }
        for (const Vertex* vertex : {&outer0, &inner0, &inner1, &outer1})
        {
            CHECK(vertex->x >= grown.left);
            CHECK(vertex->x <= grown.left + grown.width);
            CHECK(vertex->y >= grown.top);
            CHECK(vertex->y <= grown.top + grown.height);
        }
    }

    // The other winding gives the same rim.
    std::vector<OutlinePoint> reversed(outline.rbegin(), outline.rend());
    std::vector<Vertex> other;
    BuildOrbUnder(GemBox, reversed, Width, Colours, other);
    REQUIRE(other.size() == quads.size());
    for (std::size_t q = 0; q < quads.size(); q += 4)
    {
        const Vertex& outer = quads[q];
        const bool found = std::any_of(other.begin(), other.end(),
                                       [&](const Vertex& vertex)
                                       {
                                           return vertex.abgr == Colours.outer &&
                                                  std::fabs(vertex.x - outer.x) <= Near &&
                                                  std::fabs(vertex.y - outer.y) <= Near;
                                       });
        CHECK(found);
    }

    BuildOrbUnder(GemBox, outline, 0.f, Colours, quads);
    CHECK(quads.empty());
}

TEST_CASE("an SD/AG arc is the level's share of 40 even cuts, textured across the band [ui][live_gauge]")
{
    std::vector<Vertex> quads;
    SUBCASE("full: every cut, from the start to the end of the sweep")
    {
        BuildArc(ValueRing, {1.f, 0.f, 0.6f}, quads);
        REQUIRE(quads.size() == static_cast<std::size_t>(ArcSegments) * 4);
        CheckArcVertices(quads, ValueRing, 0.f, 260.f);
        CHECK(CountColoured(quads, FillGrey) == static_cast<std::size_t>(ArcSegments));
        CHECK(SweptTo(ValueRing, quads.front()) == doctest::Approx(0.f).epsilon(1e-3));
        CHECK(SweptTo(ValueRing, quads.back()) == doctest::Approx(260.f).epsilon(1e-3));
        CHECK(quads.front().v == doctest::Approx(0.05f));
        CHECK(quads.back().v == doctest::Approx(0.95f));
    }
    SUBCASE("half: 20 cuts ending at the top (270 degrees)")
    {
        BuildArc(ValueRing, {0.5f, 0.f, 0.6f}, quads);
        REQUIRE(quads.size() == 20 * 4);
        CheckArcVertices(quads, ValueRing, 0.f, 130.f);
        CHECK(quads.back().x == doctest::Approx(ValueRing.centreX).epsilon(1e-3));
        CHECK(quads.back().y == doctest::Approx(ValueRing.centreY - ValueRing.outer).epsilon(1e-3));
    }
    SUBCASE("between two cuts: the last quad ends at the level")
    {
        BuildArc(ValueRing, {0.51f, 0.f, 0.6f}, quads);
        REQUIRE(quads.size() == 21 * 4);
        CheckArcVertices(quads, ValueRing, 0.f, 0.51f * 260.f);
        CHECK(SweptTo(ValueRing, quads.back()) == doctest::Approx(0.51f * 260.f).epsilon(1e-3));
    }
    SUBCASE("a ghost carries on from the level at its shade")
    {
        BuildArc(ValueRing, {0.5f, 0.75f, 0.6f}, quads);
        REQUIRE(quads.size() == 30 * 4);
        CHECK(CountColoured(quads, FillGrey) == 20);
        CHECK(CountColoured(quads, GhostGrey) == 10);
        CheckArcVertices(quads, ValueRing, 0.f, 195.f);
        for (std::size_t q = 20 * 4; q < quads.size(); ++q)
            CHECK(SweptTo(ValueRing, quads[q]) >= 130.f - AngleNear);
    }
    SUBCASE("no ghost at or under the level")
    {
        BuildArc(ValueRing, {0.5f, 0.4f, 0.6f}, quads);
        CHECK(quads.size() == 20 * 4);
        CHECK(CountColoured(quads, GhostGrey) == 0);
        BuildArc(ValueRing, {0.5f, 0.5f, 0.6f}, quads);
        CHECK(quads.size() == 20 * 4);
    }
    SUBCASE("empty: nothing, unless a ghost still drains")
    {
        BuildArc(ValueRing, {0.f, 0.f, 0.6f}, quads);
        CHECK(quads.empty());
        BuildArc(ValueRing, {-1.f, 0.f, 0.6f}, quads);
        CHECK(quads.empty());
        BuildArc(ValueRing, {0.f, 0.25f, 0.6f}, quads);
        CHECK(quads.size() == 10 * 4);
        CHECK(CountColoured(quads, GhostGrey) == 10);
    }
    SUBCASE("a ring without a band or a sweep draws nothing")
    {
        BuildArc({31.f, 31.f, 30.f, 30.f, 140.f, 260.f}, {1.f, 0.f, 0.6f}, quads);
        CHECK(quads.empty());
        BuildArc({31.f, 31.f, 28.f, 30.5f, 140.f, 0.f}, {1.f, 0.f, 0.6f}, quads);
        CHECK(quads.empty());
    }
}

TEST_CASE("an arc's cap is its last 2 px, the art added at a tint [ui][live_gauge]")
{
    std::vector<Vertex> quads;
    const float middle = 0.5f * (ValueRing.inner + ValueRing.outer);
    const float capDegrees = 2.f / middle * DegreesPerRadian;

    BuildArcCap(ValueRing, 0.5f, 2.f, 0.3f, quads);
    REQUIRE_FALSE(quads.empty());
    CheckArcVertices(quads, ValueRing, 130.f - capDegrees, 130.f);
    CHECK(CountColoured(quads, EmptyGrey) == quads.size() / 4);
    CHECK(SweptTo(ValueRing, quads.front()) == doctest::Approx(130.f - capDegrees).epsilon(1e-3));
    CHECK(SweptTo(ValueRing, quads.back()) == doctest::Approx(130.f).epsilon(1e-3));

    // Across a cut it takes the two quads round it.
    const float level = (130.f + 0.5f * capDegrees) / 260.f;
    BuildArcCap(ValueRing, level, 2.f, 0.3f, quads);
    CHECK(quads.size() == 2 * 4);

    // A level shorter than the cap: the cap starts at the sweep's start.
    BuildArcCap(ValueRing, 0.002f, 2.f, 0.3f, quads);
    REQUIRE(quads.size() == 4);
    CheckArcVertices(quads, ValueRing, 0.f, 0.002f * 260.f);
    CHECK(SweptTo(ValueRing, quads.front()) == doctest::Approx(0.f).epsilon(1e-3));

    BuildArcCap(ValueRing, 0.f, 2.f, 0.3f, quads);
    CHECK(quads.empty());
    BuildArcCap(ValueRing, 0.5f, 0.f, 0.3f, quads);
    CHECK(quads.empty());
    BuildArcCap(ValueRing, 0.5f, 2.f, 0.f, quads);
    CHECK(quads.empty());
}

TEST_CASE("an arc's track is bronze at both edges and dark midway, added after the socket [ui][live_gauge]")
{
    constexpr Ring Track{31.f, 31.f, 27.5f, 31.f, 140.f, 260.f};
    constexpr RimColours Colours{0xFF2A465Cu, 0xFF07090Bu};
    std::vector<Vertex> quads(4, Vertex{});
    AppendArcTrack(Track, Colours, quads);
    REQUIRE(quads.size() == 4 + 2 * static_cast<std::size_t>(ArcSegments) * 4);
    for (std::size_t i = 0; i < 4; ++i)
        CHECK(quads[i].abgr == 0u);

    const float middle = 0.5f * (Track.inner + Track.outer);
    for (std::size_t i = 4; i < quads.size(); ++i)
    {
        const Vertex& vertex = quads[i];
        const float radius = RadiusOf(Track, vertex);
        CHECK(radius >= Track.inner - Near);
        CHECK(radius <= Track.outer + Near);
        CHECK(SweptTo(Track, vertex) <= 260.f + AngleNear);
        CHECK(vertex.u == 0.f);
        CHECK(vertex.v == 0.f);
        if (std::fabs(radius - middle) <= Near)
            CHECK(vertex.abgr == Colours.inner);
        else
            CHECK(vertex.abgr == Colours.outer);
    }

    const std::size_t before = quads.size();
    AppendArcTrack({31.f, 31.f, 31.f, 31.f, 140.f, 260.f}, Colours, quads);
    CHECK(quads.size() == before);
}

TEST_CASE("a gem's halo fans from its outline to the ring, cut at the group's bottom [ui][live_gauge]")
{
    const std::vector<OutlinePoint> outline = DefaultOutline();
    const float centreX = GemBox.left + GemBox.width / 2.f;
    const float centreY = GemBox.top + GemBox.height / 2.f;
    const float bottom = GemBox.top + GemBox.height;
    const HaloShape shape{centreX, centreY, 31.f, bottom};
    constexpr std::uint32_t Red = 0xFF1834FFu;
    constexpr std::uint32_t HalfRed = 0x800C1A80u;
    std::vector<Vertex> quads;
    BuildHalo(GemBox, outline, shape, Red, 0.5f, quads);
    REQUIRE(quads.size() >= 48 * 4);
    REQUIRE(quads.size() % 4 == 0);

    float lowest = 0.f;
    for (std::size_t q = 0; q < quads.size(); q += 4)
    {
        CAPTURE(q / 4);
        const Vertex& outer0 = quads[q];
        const Vertex& inner0 = quads[q + 1];
        const Vertex& inner1 = quads[q + 2];
        const Vertex& outer1 = quads[q + 3];
        CHECK(outer0.abgr == 0u);
        CHECK(outer1.abgr == 0u);
        CHECK(inner0.abgr == HalfRed);
        CHECK(inner1.abgr == HalfRed);
        for (const Vertex* vertex : {&outer0, &inner0, &inner1, &outer1})
        {
            CHECK(std::hypot(vertex->x - centreX, vertex->y - centreY) <= shape.radius + Near);
            CHECK(vertex->y <= bottom + Near);
            lowest = std::max(lowest, vertex->y);
        }
        // The inner edge is on the gem's outline, inside its box; the outer one never nearer the centre.
        for (const Vertex* inner : {&inner0, &inner1})
        {
            CHECK(inner->x >= GemBox.left - Near);
            CHECK(inner->x <= GemBox.left + GemBox.width + Near);
            CHECK(inner->y >= GemBox.top - Near);
            CHECK(inner->y <= bottom + Near);
        }
        CHECK(std::hypot(outer0.x - centreX, outer0.y - centreY) >=
              std::hypot(inner0.x - centreX, inner0.y - centreY) - Near);
    }
    // Straight down it reaches the group's bottom and no further.
    CHECK(lowest == doctest::Approx(bottom));

    BuildHalo(GemBox, outline, shape, Red, 1.f, quads);
    CHECK(CountColoured(quads, 0u) == quads.size() / 4);
    CHECK(quads[1].abgr == Red);

    BuildHalo(GemBox, outline, shape, Red, 0.f, quads);
    CHECK(quads.empty());
    BuildHalo(GemBox, outline, {centreX, centreY, 0.f, bottom}, Red, 0.5f, quads);
    CHECK(quads.empty());
}

namespace
{
// Legacy's hotkey slot and current-skill slot at 1 px per dp (main_frame.rcss), the second away from the origin.
constexpr Box HotkeySlot{0.f, 0.f, 32.f, 38.f};
constexpr Box CurrentSlot{170.f, 8.f, 20.f, 28.f};
constexpr std::uint32_t SweepShade = 0xCC030406u; // #060403cc

float TriangleArea(const Vertex& a, const Vertex& b, const Vertex& c)
{
    return 0.5f * std::fabs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
}

// What a mesh's quads cover, both triangles of each (indices 0,1,2 and 0,2,3).
float MeshArea(const std::vector<Vertex>& quads)
{
    float area = 0.f;
    for (std::size_t q = 0; q + 3 < quads.size(); q += 4)
        area += TriangleArea(quads[q], quads[q + 1], quads[q + 2]) + TriangleArea(quads[q], quads[q + 2], quads[q + 3]);
    return area;
}

// Whether (x, y) lies in one of the quads' first triangles (the sweep's second ones are empty).
bool Covers(const std::vector<Vertex>& quads, float x, float y)
{
    for (std::size_t q = 0; q + 3 < quads.size(); q += 4)
    {
        const Vertex& a = quads[q];
        const Vertex& b = quads[q + 1];
        const Vertex& c = quads[q + 2];
        const auto side = [&](const Vertex& from, const Vertex& to)
        { return (to.x - from.x) * (y - from.y) - (to.y - from.y) * (x - from.x); };
        const float ab = side(a, b);
        const float bc = side(b, c);
        const float ca = side(c, a);
        const bool anyNegative = ab < 0.f || bc < 0.f || ca < 0.f;
        const bool anyPositive = ab > 0.f || bc > 0.f || ca > 0.f;
        if (!(anyNegative && anyPositive) && TriangleArea(a, b, c) > 0.f)
            return true;
    }
    return false;
}
} // namespace

TEST_CASE("a cooldown sweep covers what is left of the turn from 12 o'clock, cut to the slot [ui][live_gauge]")
{
    std::vector<Vertex> quads;
    for (const Box& slot : {HotkeySlot, CurrentSlot})
    {
        const float area = slot.width * slot.height;
        const float centreX = slot.left + slot.width / 2.f;
        const float centreY = slot.top + slot.height / 2.f;
        for (const float remaining : {0.25f, 0.5f, 0.75f, 0.99f, 1.f})
        {
            CAPTURE(slot.left);
            CAPTURE(remaining);
            BuildSweep(slot, remaining, SweepShade, quads);
            REQUIRE_FALSE(quads.empty());
            CHECK(quads.size() % 4 == 0);
            for (const Vertex& vertex : quads)
            {
                CHECK(vertex.x >= slot.left - Near);
                CHECK(vertex.x <= slot.left + slot.width + Near);
                CHECK(vertex.y >= slot.top - Near);
                CHECK(vertex.y <= slot.top + slot.height + Near);
                CHECK(vertex.abgr == SweepShade);
            }
            CHECK(std::fabs(MeshArea(quads) / area - remaining) <= 0.02f);
        }

        // The quarters a hand from 12 o'clock has passed are open, the rest dark.
        const float quarterX = slot.width / 4.f;
        const float quarterY = slot.height / 4.f;
        BuildSweep(slot, 0.75f, SweepShade, quads);
        CHECK_FALSE(Covers(quads, centreX + quarterX, centreY - quarterY));
        CHECK(Covers(quads, centreX + quarterX, centreY + quarterY));
        CHECK(Covers(quads, centreX - quarterX, centreY + quarterY));
        CHECK(Covers(quads, centreX - quarterX, centreY - quarterY));
        BuildSweep(slot, 0.25f, SweepShade, quads);
        CHECK_FALSE(Covers(quads, centreX + quarterX, centreY - quarterY));
        CHECK_FALSE(Covers(quads, centreX + quarterX, centreY + quarterY));
        CHECK_FALSE(Covers(quads, centreX - quarterX, centreY + quarterY));
        CHECK(Covers(quads, centreX - quarterX, centreY - quarterY));
        // Just after the start only a sliver right of 12 o'clock is open.
        BuildSweep(slot, 0.99f, SweepShade, quads);
        CHECK_FALSE(Covers(quads, centreX + 0.5f, slot.top + 1.f));
        CHECK(Covers(quads, centreX - 0.5f, slot.top + 1.f));
        CHECK(Covers(quads, centreX + slot.width / 2.f - 1.f, slot.top + 1.f));

        BuildSweep(slot, 1.f, SweepShade, quads);
        CHECK(MeshArea(quads) == doctest::Approx(area).epsilon(1e-4));
        BuildSweep(slot, 3.f, SweepShade, quads);
        CHECK(MeshArea(quads) == doctest::Approx(area).epsilon(1e-4));
        BuildSweep(slot, 0.f, SweepShade, quads);
        CHECK(quads.empty());
        BuildSweep(slot, -0.5f, SweepShade, quads);
        CHECK(quads.empty());
    }
    BuildSweep({0.f, 0.f, 0.f, 38.f}, 0.5f, SweepShade, quads);
    CHECK(quads.empty());
}

TEST_CASE("the EXP spark is an 8-triangle fan, bright at its centre and dark at its rim [ui][live_gauge]")
{
    constexpr std::uint32_t Gold = 0xFF78D7FFu; // #ffd778
    constexpr std::uint32_t HalfGold = 0x803C6C80u;
    constexpr float CentreX = 120.f;
    constexpr float CentreY = 2.f;
    constexpr float Radius = 5.f;
    std::vector<Vertex> quads;
    BuildGlow(CentreX, CentreY, Radius, Gold, 0.5f, quads);
    REQUIRE(quads.size() == static_cast<std::size_t>(GlowSegments) * 4);
    for (std::size_t q = 0; q < quads.size(); q += 4)
    {
        CAPTURE(q / 4);
        for (const std::size_t centre : {q, q + 3})
        {
            CHECK(quads[centre].x == doctest::Approx(CentreX));
            CHECK(quads[centre].y == doctest::Approx(CentreY));
            CHECK(quads[centre].abgr == HalfGold);
        }
        for (const std::size_t rim : {q + 1, q + 2})
        {
            CHECK(std::hypot(quads[rim].x - CentreX, quads[rim].y - CentreY) == doctest::Approx(Radius));
            CHECK(quads[rim].abgr == 0u);
        }
    }
    // Together the triangles make the whole octagon round the centre.
    CHECK(MeshArea(quads) == doctest::Approx(2.f * std::sqrt(2.f) * Radius * Radius).epsilon(1e-4));

    BuildGlow(CentreX, CentreY, Radius, Gold, 2.f, quads);
    CHECK(quads[0].abgr == Gold);
    BuildGlow(CentreX, CentreY, Radius, Gold, 0.f, quads);
    CHECK(quads.empty());
    BuildGlow(CentreX, CentreY, 0.f, Gold, 1.f, quads);
    CHECK(quads.empty());
}

TEST_CASE("the level-up band crosses the EXP bar left to right, cut to the bar [ui][live_gauge]")
{
    constexpr Box Track{2.f, 3.f, 629.f, 4.f};
    constexpr float HalfWidth = 16.f;
    constexpr std::uint32_t Gold = 0xFF78D7FFu;
    const std::uint32_t peakRed = 204u; // 255 * 0.8
    std::vector<Vertex> quads;

    // Midway: both halves whole, bright on the line, dark at the edges.
    AppendBand(Track, 0.5f, HalfWidth, Gold, 0.8f, quads);
    REQUIRE(quads.size() == 8);
    const float line = Track.left - HalfWidth + 0.5f * (Track.width + 2.f * HalfWidth);
    CHECK(quads[0].x == doctest::Approx(line - HalfWidth));
    CHECK(Channel(quads[0].abgr) == 0u);
    CHECK(quads[3].x == doctest::Approx(line));
    CHECK(Channel(quads[3].abgr) == peakRed);
    CHECK(quads[4].x == doctest::Approx(line));
    CHECK(Channel(quads[4].abgr) == peakRed);
    CHECK(quads[7].x == doctest::Approx(line + HalfWidth));
    CHECK(Channel(quads[7].abgr) == 0u);
    for (const Vertex& vertex : quads)
    {
        CHECK((vertex.y == doctest::Approx(Track.top) || vertex.y == doctest::Approx(Track.top + Track.height)));
    }

    // It moves right as the phase runs, and is cut at the bar's ends with the strength it has there.
    float lastLine = -1.0e9f;
    for (const float phase : {0.01f, 0.2f, 0.4f, 0.6f, 0.8f, 0.99f})
    {
        CAPTURE(phase);
        quads.clear();
        AppendBand(Track, phase, HalfWidth, Gold, 0.8f, quads);
        REQUIRE_FALSE(quads.empty());
        CHECK(quads.size() % 4 == 0);
        float brightestX = 0.f;
        std::uint32_t brightest = 0u;
        for (const Vertex& vertex : quads)
        {
            CHECK(vertex.x >= Track.left - Near);
            CHECK(vertex.x <= Track.left + Track.width + Near);
            if (Channel(vertex.abgr) >= brightest)
            {
                brightest = Channel(vertex.abgr);
                brightestX = vertex.x;
            }
        }
        CHECK(brightest > 0u);
        CHECK(brightest <= peakRed);
        CHECK(brightestX > lastLine);
        lastLine = brightestX;
    }
    quads.clear();
    AppendBand(Track, 0.01f, HalfWidth, Gold, 0.8f, quads);
    REQUIRE(quads.size() == 4);
    CHECK(quads[0].x == doctest::Approx(Track.left));
    CHECK(Channel(quads[0].abgr) > 0u);
    CHECK(Channel(quads[0].abgr) < peakRed);

    // Whole outside the bar at the ends, and nothing outside 0..1: nothing added.
    quads.assign(4, Vertex{});
    for (const float phase : {0.f, 1.f, -0.1f, 1.1f})
    {
        CAPTURE(phase);
        AppendBand(Track, phase, HalfWidth, Gold, 0.8f, quads);
        CHECK(quads.size() == 4);
    }
    AppendBand(Track, 0.5f, HalfWidth, Gold, 0.f, quads);
    CHECK(quads.size() == 4);
    // Appended after what is there.
    AppendBand(Track, 0.5f, HalfWidth, Gold, 0.8f, quads);
    CHECK(quads.size() == 12);
}
