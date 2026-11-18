#pragma once

// The ground pick (RenderTerrain(true)) tests the mouse segment against terrain tiles. A hit lies
// on the segment and inside its tile's square, so a block of tiles whose square the segment's
// ground projection does not cross cannot hold a hit and is skipped.

#include <algorithm>
#include <utility>

namespace Render::Terrain::Pick
{
// The pick segment projected on the ground plane (world X and Y).
struct Segment2D
{
    float startX = 0.f;
    float startY = 0.f;
    float endX = 0.f;
    float endY = 0.f;
};

// Narrows [enter, leave] (segment parameters) to the part of the segment between lo and hi on one
// axis. False when nothing of it is left.
inline bool ClipAxis(float start, float delta, float lo, float hi, float& enter, float& leave)
{
    if (delta == 0.f)
    {
        return start >= lo && start <= hi;
    }
    float first = (lo - start) / delta;
    float second = (hi - start) / delta;
    if (first > second)
    {
        std::swap(first, second);
    }
    enter = std::max(enter, first);
    leave = std::min(leave, second);
    return enter <= leave;
}

// Whether any point of the segment lies in the box [minX, maxX] x [minY, maxY] (edges included).
inline bool SegmentCrossesBox(const Segment2D& segment, float minX, float minY, float maxX, float maxY)
{
    float enter = 0.f;
    float leave = 1.f;
    return ClipAxis(segment.startX, segment.endX - segment.startX, minX, maxX, enter, leave) &&
           ClipAxis(segment.startY, segment.endY - segment.startY, minY, maxY, enter, leave);
}
} // namespace Render::Terrain::Pick
