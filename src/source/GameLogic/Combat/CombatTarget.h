#pragma once

class CHARACTER;

// Resolves the current attack target tile from the selection (or the ground
// cursor) and records it on the character. Extracted from ZzzInterface.cpp.
namespace GameLogic::Combat
{
    bool CheckTarget(CHARACTER* c);

    // The pad's Teleport, which has no pointer in the world: with nothing selected, the cast goes to
    // this tile instead of the ground under the mouse pointer, until ClearGroundAim.
    void SetGroundAim(int tileX, int tileY);
    void ClearGroundAim();
}
