#include "UI/Mobile/TouchTarget.h"

#include <algorithm>
#include <cmath>

namespace UI::Mobile::TouchTarget
{
float DistanceOutside(const Box& box, float x, float y)
{
    const float dx = std::max({box.left - x, 0.f, x - box.right});
    const float dy = std::max({box.top - y, 0.f, y - box.bottom});
    return std::hypot(dx, dy);
}

int Closest(std::span<const Box> boxes, float x, float y, float slop)
{
    int closest = NoBox;
    float closestDistance = 0.f;
    for (int index = 0; index < static_cast<int>(boxes.size()); ++index)
    {
        const Box& box = boxes[index];
        if (!box.shown)
            continue;
        const float distance = DistanceOutside(box, x, y);
        if (distance <= 0.f)
            return index;
        if (distance <= slop && (closest == NoBox || distance < closestDistance))
        {
            closest = index;
            closestDistance = distance;
        }
    }
    return closest;
}
} // namespace UI::Mobile::TouchTarget
