#include "GameLogic/Touch/SteerHold.h"

#include <algorithm>
#include <cstdlib>

namespace GameLogic::Touch
{
namespace
{
// Tiles a step may cover at once without counting as a jump, plus the fastest a mount runs per
// second, with room to spare: a slow frame on a phone is no gate. A wizard's Teleport stays below.
constexpr int JumpTiles = 8;
constexpr double RunTilesPerSecond = 12.0;
} // namespace

bool IsJump(const HeroPlace& from, const HeroPlace& to)
{
    const int tiles = std::max(std::abs(to.tileX - from.tileX), std::abs(to.tileY - from.tileY));
    const double seconds = std::max(0.0, to.seconds - from.seconds);
    return tiles > JumpTiles + RunTilesPerSecond * seconds;
}

bool SteerHold::MayWalk(const HeroPlace& place)
{
    if (m_waiting)
        return false;
    if (m_held && (place.world != m_last.world || IsJump(m_last, place)))
    {
        m_waiting = true;
        return false;
    }
    m_held = true;
    m_last = place;
    return true;
}

void SteerHold::Pause()
{
    if (m_held)
        m_waiting = true;
}

void SteerHold::Release()
{
    m_held = false;
    m_waiting = false;
}
} // namespace GameLogic::Touch
