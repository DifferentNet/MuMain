// Mouse-free movement of the controlled character.
//
// Extracted from MUHelper/MuHelper.cpp so the auto-helper and the control
// socket walk through the same code the mouse does: the client's own path
// finder fills Hero->Path and the walk packet goes out unchanged.
#pragma once

#include <cstdint>
#include <string_view>

namespace GameLogic::Automation
{
enum class MoveResult : std::uint8_t
{
    // Already standing on (or within a tile of) the target.
    Arrived,
    // A path was found and the walk packet was sent.
    Walking,
    // The path finder found no way there.
    NoPath,
};

// Distance, in tiles, at which the character counts as arrived. The value
// the auto-helper has always used for its own regroup move.
inline constexpr float ArrivalTolerance = 1.5f;

[[nodiscard]] constexpr std::string_view ResultName(MoveResult result)
{
    switch (result)
    {
    case MoveResult::Arrived:
        return "arrived";
    case MoveResult::Walking:
        return "walking";
    case MoveResult::NoPath:
        return "no_path";
    }
    return "unknown";
}

// Whether a walk back somewhere should be given up for a while: there is no way there, or it has gone on
// `sinceCloser` (any time unit) without getting a tile closer, for at least `patience`. Monsters stand on their
// tiles, so a ring of them shuts every way out (MU Helper among monsters, 2026-11-03).
[[nodiscard]] constexpr bool ShouldGiveUpWalk(MoveResult result, long long sinceCloser, long long patience)
{
    return result == MoveResult::NoPath || (result == MoveResult::Walking && sinceCloser >= patience);
}

// How far a walk back still has to go, for telling whether it gets closer. A path that ends on the spot counts its
// steps, so a detour round a wall is getting closer too. A path that doesn't, the path finder's way to the nearest
// free tile when monsters stand all round or the spot is beyond its look (MAX_PATH_FIND), says nothing about the
// spot: it counts the straight line from the character's tile instead, put above any whole path. Measured by the
// partial path's steps, a short way to a free tile among monsters looked like nearly there, so the walk never got
// given up and the helper stood walking instead of fighting until it was restarted (players, Kanturu 2, 2026-11-04).
inline constexpr int PartialPathGap = 1000;

[[nodiscard]] constexpr int WalkGap(bool pathEndsOnSpot, int pathSteps, int straightTiles)
{
    return pathEndsOnSpot ? pathSteps : PartialPathGap + straightTiles;
}

// Whether a walk got closer this round: its gap (WalkGap) is the smallest seen in this attempt.
[[nodiscard]] constexpr bool IsWalkProgress(int gap, int bestGap)
{
    return gap < bestGap;
}

// Walks towards the map tile, one path per call, exactly as a click there
// would. Call again on later frames until it reports Arrived.
MoveResult WalkTo(int tileX, int tileY);

// The length (in tiles, the start included) a walk under way is cut to so it
// ends as soon as it can: on the tile the character is stepping onto.
// `currentPath` is the index of the tile it last left and `pathNum` the
// path's length, as PATH_t keeps them; a walk that ends there anyway keeps
// its length.
[[nodiscard]] constexpr int StopPathLength(int currentPath, int pathNum)
{
    return pathNum < currentPath + 2 ? pathNum : currentPath + 2;
}

// Ends the walk under way on the tile the character is stepping onto: the
// step it is taking is finished, so it stands on a tile, and the server is
// told where (a walk of no steps, as when a skill interrupts a walk).
// Clearing the path alone is not a stop: the engine keeps walking a moving
// character straight ahead while it has no path left.
void StopAtNextTile();
} // namespace GameLogic::Automation
