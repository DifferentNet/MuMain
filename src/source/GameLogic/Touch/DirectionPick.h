// Picks a target the way a stick points (pad round 3, 2026-11-13).
//
// The right stick used to step through the foes by distance whichever way it leaned: right or down the
// next further one, left or up the next nearer. Now it picks the one that lies the way it leans, as a
// d-pad steps between buttons: from the one lit (or the hero, with none lit) the nearest within a cone
// around the lean, and with none there the one at the smallest angle. Held, it keeps going that way
// and stops at the last one. The left stick's walk tilts the automatic choice the walk's way too.
#pragma once

#include <vector>

namespace GameLogic::Touch::DirectionPick
{
constexpr int NoKey = -1;
// Half the cone's width: a lean picks among those within 45 degrees of it first.
constexpr float ConeHalfDegrees = 45.f;

struct Candidate
{
    int key = NoKey;
    // Where it stands, in the map's tiles.
    float x = 0.f;
    float y = 0.f;
};

// The candidate the way (dirX, dirY) points from (originX, originY), never `skip`: within the cone
// the one with the least distance stretched by its angle off the lean; with none in the cone and
// `fallBack`, the one at the smallest angle (the nearer of two alike); else NoKey.
[[nodiscard]] int Pick(const std::vector<Candidate>& candidates, float originX, float originY, float dirX,
                       float dirY, int skip, bool fallBack);

// A distance stretched by how far (dx, dy) turns off the way (dirX, dirY): unchanged straight ahead,
// up to half again to the side and twice behind. Unchanged for no way at all.
[[nodiscard]] float Biased(float distance, float dx, float dy, float dirX, float dirY);
} // namespace GameLogic::Touch::DirectionPick
