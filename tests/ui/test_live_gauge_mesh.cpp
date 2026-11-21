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
