#include "GameLogic/Touch/DirectionPick.h"

#include <cmath>
#include <limits>

namespace GameLogic::Touch::DirectionPick
{
namespace
{
// The cosine of the angle between (dx, dy) and (dirX, dirY); 1 when either is no way at all.
float CosineTo(float dx, float dy, float dirX, float dirY)
{
    const float length = std::hypot(dx, dy) * std::hypot(dirX, dirY);
    if (length < 1e-6f)
        return 1.f;
    return (dx * dirX + dy * dirY) / length;
}
} // namespace

int Pick(const std::vector<Candidate>& candidates, float originX, float originY, float dirX, float dirY,
         int skip, bool fallBack)
{
    const float coneCosine = std::cos(ConeHalfDegrees * 3.14159265f / 180.f);
    int best = NoKey;
    float bestScore = std::numeric_limits<float>::max();
    int nearestAngle = NoKey;
    float nearestCosine = -2.f;
    float nearestAngleDistance = 0.f;
    for (const Candidate& candidate : candidates)
    {
        if (candidate.key == skip || candidate.key == NoKey)
            continue;
        const float dx = candidate.x - originX;
        const float dy = candidate.y - originY;
        const float distance = std::hypot(dx, dy);
        // One standing on the origin lies no way at all: only a fall-back takes it.
        const float cosine = distance < 1e-3f ? -1.f : CosineTo(dx, dy, dirX, dirY);
        if (cosine >= coneCosine)
        {
            // Off the lean by the cone's edge, a candidate counts about 1.6 times as far.
            const float score = distance * (1.f + 2.f * (1.f - cosine));
            if (score < bestScore)
            {
                bestScore = score;
                best = candidate.key;
            }
        }
        if (cosine > nearestCosine + 1e-4f || (std::fabs(cosine - nearestCosine) <= 1e-4f && distance < nearestAngleDistance))
        {
            nearestCosine = cosine;
            nearestAngle = candidate.key;
            nearestAngleDistance = distance;
        }
    }
    if (best != NoKey)
        return best;
    return fallBack ? nearestAngle : NoKey;
}

float Biased(float distance, float dx, float dy, float dirX, float dirY)
{
    if (std::hypot(dirX, dirY) < 1e-6f || std::hypot(dx, dy) < 1e-6f)
        return distance;
    return distance * (1.f + 0.5f * (1.f - CosineTo(dx, dy, dirX, dirY)));
}
} // namespace GameLogic::Touch::DirectionPick
