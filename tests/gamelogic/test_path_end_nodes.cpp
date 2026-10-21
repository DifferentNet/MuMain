// Tests of the path finding's end tiles (PATH::SetEndNodes): a walk to a target within a range ends on any tile in
// that range around it. Tiles beyond the map's edge used to be checked by their index alone, so one past the left
// edge was a tile at the right end of the row above, and a walk could end there, across the map.

#include "stdafx.h"

#include "doctest.h"

#include "Engine/Pathing/ZzzPath.h"

#include <array>
#include <cmath>

namespace
{
constexpr int kMapSize = 10;

struct PathEnd
{
    int x;
    int y;
};

// Finds a path on an open map the way the client does (wall check first, characters are walls) and returns
// where it ends.
PathEnd FindPathEnd(int xStart, int yStart, int xTarget, int yTarget, float range)
{
    std::array<WORD, kMapSize * kMapSize> map{};
    PATH path;
    path.SetMapDimensions(kMapSize, kMapSize, map.data());
    REQUIRE(path.FindPath(xStart, yStart, xTarget, yTarget, true, TW_CHARACTER, false, range));
    REQUIRE(path.GetPath() > 0);
    const int last = path.GetPath() - 1;
    return {path.GetPathX()[last], path.GetPathY()[last]};
}

float Distance(const PathEnd& end, int x, int y)
{
    return std::sqrt(static_cast<float>((end.x - x) * (end.x - x) + (end.y - y) * (end.y - y)));
}
} // namespace

TEST_CASE("A walk to a target at the left edge ends in range of the target, not across the map")
{
    // The range of (0, 3) reaches past the left edge; those tiles used to be (9, 1), (9, 2), (9, 3) and (8, 2),
    // right next to the start.
    const PathEnd end = FindPathEnd(8, 1, 0, 3, 2.0f);
    CHECK(Distance(end, 0, 3) <= 2.0f);
    CHECK(end.x <= 2);
}

TEST_CASE("A walk to a target at the top edge ends in range of the target")
{
    const PathEnd end = FindPathEnd(5, 8, 5, 0, 2.0f);
    CHECK(Distance(end, 5, 0) <= 2.0f);
}

TEST_CASE("A walk to a target in the middle of the map stops at the edge of the range")
{
    const PathEnd end = FindPathEnd(1, 5, 7, 5, 2.0f);
    CHECK(Distance(end, 7, 5) <= 2.0f);
    CHECK(end.x == 5);
}
