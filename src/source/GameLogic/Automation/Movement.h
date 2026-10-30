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
