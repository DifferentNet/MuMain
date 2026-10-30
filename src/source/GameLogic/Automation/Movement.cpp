#include "stdafx.h"
#include "GameLogic/Automation/Movement.h"

#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"

#include <mutex>

// Movement globals are defined in ZzzInterface.cpp.
extern int TargetX;
extern int TargetY;

namespace GameLogic::Automation
{
MoveResult WalkTo(int tileX, int tileY)
{
    // The arrival test and the path finder both read the target globals, so
    // they are set first; the movement mode follows only when a walk is
    // actually starting, so a refused call leaves the character's current
    // behaviour alone.
    TargetX = tileX;
    TargetY = tileY;

    if (CheckTile(Hero, &Hero->Object, ArrivalTolerance))
    {
        return MoveResult::Arrived;
    }

    if (!PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &Hero->Path))
    {
        return MoveResult::NoPath;
    }

    Hero->MovementType = MOVEMENT_MOVE;
    SendMove(Hero, &Hero->Object);
    return MoveResult::Walking;
}

void StopAtNextTile()
{
    if (Hero == nullptr || !Hero->Movement)
    {
        return;
    }

    BYTE stopX = Hero->PositionX;
    BYTE stopY = Hero->PositionY;
    bool shortened = false;
    bool walkable = false;
    {
        std::lock_guard<SpinLock> guard(Hero->Path.Lock);
        PATH_t& path = Hero->Path;
        const int length = StopPathLength(path.CurrentPath, path.PathNum);
        // A path of one tile has no step left to finish.
        walkable = length >= 2;
        if (walkable)
        {
            shortened = length < path.PathNum;
            path.PathNum = static_cast<unsigned char>(length);
            stopX = path.PathX[length - 1];
            stopY = path.PathY[length - 1];
        }
    }

    if (!walkable)
    {
        // Moving with no path to follow (another stop cleared it): stand still at once.
        Hero->Movement = false;
        SetPlayerStop(Hero);
    }
    else if (!shortened)
    {
        // The walk ends on that tile anyway, where the server already expects it.
        return;
    }

    // The tile lies on the walk the server was sent, so it stops the character
    // there too (a stop elsewhere would be refused and the walk kept).
    SendCharacterMove(Hero->Key, Hero->Object.Angle[2], 1, &stopX, &stopY, stopX, stopY);
}
} // namespace GameLogic::Automation
