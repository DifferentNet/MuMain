#include <cstring>

#include <doctest.h>

#include "Render/Terrain/GroundQuadRun.h"

using Render::Terrain::GroundQuadRun;

namespace
{
void MakeQuad(float seed, mu::Vertex3D (&quad)[GroundQuadRun::kQuadVertices])
{
    for (std::size_t i = 0; i < GroundQuadRun::kQuadVertices; ++i)
    {
        const float f = seed + static_cast<float>(i);
        quad[i] = {f, f + 1.f, f + 2.f, 0.f, 0.f, 1.f, f * 0.5f, f * 0.25f, 0xFF000000u + static_cast<std::uint32_t>(i)};
    }
}
} // namespace

TEST_CASE("a ground quad run starts empty [render][terrain]")
{
    // Static: a run holds kMaxVertices vertices, too big for the test stack.
    static GroundQuadRun run;
    run.Clear();
    CHECK(run.Empty());
    CHECK_FALSE(run.Full());
    CHECK(run.Vertices().empty());
}

TEST_CASE("a ground quad run keeps the appended vertices in order [render][terrain]")
{
    static GroundQuadRun run;
    run.Clear();
    mu::Vertex3D first[GroundQuadRun::kQuadVertices];
    mu::Vertex3D second[GroundQuadRun::kQuadVertices];
    MakeQuad(10.f, first);
    MakeQuad(20.f, second);
    run.Append(first);
    run.Append(second);

    const auto vertices = run.Vertices();
    REQUIRE(vertices.size() == 2 * GroundQuadRun::kQuadVertices);
    CHECK(std::memcmp(vertices.data(), first, sizeof(first)) == 0);
    CHECK(std::memcmp(vertices.data() + GroundQuadRun::kQuadVertices, second, sizeof(second)) == 0);
}

TEST_CASE("a ground quad run is full at kMaxQuads and clears for the next run [render][terrain]")
{
    static GroundQuadRun run;
    run.Clear();
    mu::Vertex3D quad[GroundQuadRun::kQuadVertices];
    MakeQuad(1.f, quad);
    for (std::size_t i = 0; i + 1 < GroundQuadRun::kMaxQuads; ++i)
        run.Append(quad);
    CHECK_FALSE(run.Full());
    run.Append(quad);
    CHECK(run.Full());
    CHECK(run.Vertices().size() == GroundQuadRun::kMaxVertices);

    // A full run takes no more: the caller flushes first.
    run.Append(quad);
    CHECK(run.Vertices().size() == GroundQuadRun::kMaxVertices);

    run.Clear();
    CHECK(run.Empty());
    CHECK(run.Vertices().empty());
}
