// Tests of where a walk ends when it is stopped early (GameLogic::Automation::StopPathLength): on the tile the
// character is stepping onto. The joystick used to clear the path instead, and the engine walked a moving
// character straight on for good (a Pixel 9 Pro XL, 2026-10-30).

#include <doctest.h>

#include "GameLogic/Automation/Movement.h"

#include <algorithm>

using GameLogic::Automation::StopPathLength;

namespace
{
// The longest path the client's path finder makes (MAX_PATH_FIND).
constexpr int LongestPath = 15;
} // namespace

TEST_CASE("A stopped walk ends on the tile being stepped onto [gamelogic][walk]")
{
    // The joystick's walk: the start and four steps. Just set off, the character is stepping onto tile 1.
    CHECK(StopPathLength(0, 5) == 2);
    // Between tiles 1 and 2: it ends on tile 2, two tiles short of where it was going.
    CHECK(StopPathLength(1, 5) == 3);
}

TEST_CASE("A walk on its last step keeps its length [gamelogic][walk]")
{
    CHECK(StopPathLength(3, 5) == 5);
    // Already on the last tile.
    CHECK(StopPathLength(4, 5) == 5);
}

TEST_CASE("No path stays no path [gamelogic][walk]")
{
    CHECK(StopPathLength(0, 0) == 0);
    CHECK(StopPathLength(0, 1) == 1);
}

TEST_CASE("A stopped walk is never longer and still finishes its step [gamelogic][walk]")
{
    for (int pathNum = 2; pathNum <= LongestPath; ++pathNum)
    {
        for (int currentPath = 0; currentPath < pathNum; ++currentPath)
        {
            CAPTURE(pathNum);
            CAPTURE(currentPath);
            const int length = StopPathLength(currentPath, pathNum);
            CHECK(length <= pathNum);
            // The tile being stepped onto (or the last one) stays on the path, so the step under way finishes.
            CHECK(length - 1 >= std::min(currentPath + 1, pathNum - 1));
            // Nothing beyond it is walked.
            CHECK(length - 1 <= currentPath + 1);
        }
    }
}
