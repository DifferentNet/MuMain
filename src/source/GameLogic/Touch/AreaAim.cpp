#include "GameLogic/Touch/AreaAim.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace GameLogic::Touch::AreaAim
{
namespace
{
float DistanceBetween(float ax, float ay, float bx, float by)
{
    return std::hypot(ax - bx, ay - by);
}

bool InArea(const Foe& foe, const Foe& centre, AreaRule rule, float size)
{
    const int dx = std::abs(foe.tileX - centre.tileX);
    const int dy = std::abs(foe.tileY - centre.tileY);
    if (rule == AreaRule::Square)
        return static_cast<float>(std::max(dx, dy)) <= size;
    return DistanceBetween(static_cast<float>(dx), static_cast<float>(dy), 0.f, 0.f) < size;
}

// How many of `foes` of the centre's kind stand in its area, itself included.
int CountAround(std::span<const Foe> foes, const Foe& centre, AreaRule rule, float size)
{
    int count = 0;
    for (const Foe& foe : foes)
    {
        if (foe.player == centre.player && InArea(foe, centre, rule, size))
            ++count;
    }
    return count;
}

const Foe* FindFoe(std::span<const Foe> foes, int key)
{
    if (key == NoKey)
        return nullptr;
    for (const Foe& foe : foes)
    {
        if (foe.key == key)
            return &foe;
    }
    return nullptr;
}
} // namespace

int PackCentre(std::span<const Foe> foes, int heroX, int heroY, AreaRule rule, float size, int litKey)
{
    const Foe* lit = FindFoe(foes, litKey);
    const bool players = lit != nullptr && lit->player;
    const float fromX = static_cast<float>(lit != nullptr ? lit->tileX : heroX);
    const float fromY = static_cast<float>(lit != nullptr ? lit->tileY : heroY);
    int bestKey = NoKey;
    int bestCount = 0;
    float bestDistance = std::numeric_limits<float>::max();
    for (const Foe& foe : foes)
    {
        if (!foe.castable || foe.player != players)
            continue;
        const int count = CountAround(foes, foe, rule, size);
        const float distance = DistanceBetween(static_cast<float>(foe.tileX), static_cast<float>(foe.tileY), fromX, fromY);
        if (count > bestCount || (count == bestCount && distance < bestDistance))
        {
            bestKey = foe.key;
            bestCount = count;
            bestDistance = distance;
        }
    }
    return bestKey;
}

std::optional<Step> AlongLine(float dirX, float dirY, int tiles, float range)
{
    const float length = std::hypot(dirX, dirY);
    if (length < 1e-6f || tiles < 1)
        return std::nullopt;
    const Step step{static_cast<int>(std::lround(dirX / length * static_cast<float>(tiles))),
                    static_cast<int>(std::lround(dirY / length * static_cast<float>(tiles)))};
    if (step.dx == 0 && step.dy == 0)
        return std::nullopt;
    if (std::hypot(static_cast<float>(step.dx), static_cast<float>(step.dy)) > range)
        return std::nullopt;
    return step;
}
} // namespace GameLogic::Touch::AreaAim
