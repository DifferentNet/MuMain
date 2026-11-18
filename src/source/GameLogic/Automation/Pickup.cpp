#include "stdafx.h"
#include "GameLogic/Automation/Pickup.h"

#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzObject.h"
#include "Network/Server/WSclient.h"

#include <cmath>

// Movement/target globals are defined in ZzzInterface.cpp.
extern int TargetX;
extern int TargetY;

namespace GameLogic::Automation
{
// Straight-line tile distance, rounded up, as the auto-helper measures it.
static int TileDistance(int fromX, int fromY, int toX, int toY)
{
    const int dx = fromX - toX;
    const int dy = fromY - toY;
    return static_cast<int>(std::ceil(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
}

// Sends the request for the drop, unless another one still waits for its answer.
static PickupResult RequestPickup(int itemIndex)
{
    // One pickup at a time: the server answers with the item, and SendGetItem is what remembers which
    // request is outstanding.
    if (SendGetItem != -1)
    {
        return PickupResult::Busy;
    }

    SendGetItem = itemIndex;
    SocketClient->ToGameServer()->SendPickupItemRequest(itemIndex);
    return PickupResult::Requested;
}

PickupResult PickUpItemInPlace(int itemIndex, int maximumDistance)
{
    if (itemIndex < 0 || itemIndex >= MAX_ITEMS || !Items[itemIndex].Object.Live)
    {
        return PickupResult::Gone;
    }

    const OBJECT& dropObject = Items[itemIndex].Object;
    const int dropX = (int)(dropObject.Position[0] / TERRAIN_SCALE);
    const int dropY = (int)(dropObject.Position[1] / TERRAIN_SCALE);
    if (TileDistance(Hero->PositionX, Hero->PositionY, dropX, dropY) > maximumDistance)
    {
        return PickupResult::TooFar;
    }
    return RequestPickup(itemIndex);
}

PickupResult PickUpItem(int itemIndex, int maximumDistance)
{
    if (itemIndex < 0 || itemIndex >= MAX_ITEMS)
    {
        return PickupResult::Gone;
    }

    ITEM_t* drop = &Items[itemIndex];
    if (!drop->Object.Live)
    {
        return PickupResult::Gone;
    }

    const int dropX = (int)(drop->Object.Position[0] / TERRAIN_SCALE);
    const int dropY = (int)(drop->Object.Position[1] / TERRAIN_SCALE);

    if (TileDistance(Hero->PositionX, Hero->PositionY, dropX, dropY) > maximumDistance)
    {
        // Answered before the client's own move target is touched: a refused
        // pickup must not redirect a walk that is already under way.
        return PickupResult::TooFar;
    }

    TargetX = dropX;
    TargetY = dropY;

    if (!CheckTile(Hero, &Hero->Object, PickupReach))
    {
        if (!PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &Hero->Path))
        {
            return PickupResult::NoPath;
        }

        Hero->MovementType = MOVEMENT_MOVE;
        SendMove(Hero, &Hero->Object);
        return PickupResult::Approaching;
    }

    // Gated here and not before the walk, as the helper this was extracted
    // from gated it: approaching the next drop while an answer is
    // outstanding is what keeps the helper moving.
    return RequestPickup(itemIndex);
}
} // namespace GameLogic::Automation
