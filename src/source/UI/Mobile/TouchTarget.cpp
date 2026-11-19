#include "UI/Mobile/TouchTarget.h"

#include <algorithm>
#include <cmath>

namespace UI::Mobile::TouchTarget
{
namespace
{
// How far in from a box's corners its probes lie, a share of its width and height: just inside a circle
// that fills the box (1 - 1/sqrt(2), halved, is 0.146).
constexpr float ProbeInset = 0.15f;
} // namespace

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

std::array<Point, ProbeCount> Probes(const Box& box)
{
    const float width = box.right - box.left;
    const float height = box.bottom - box.top;
    const float left = box.left + width * ProbeInset;
    const float right = box.right - width * ProbeInset;
    const float top = box.top + height * ProbeInset;
    const float bottom = box.bottom - height * ProbeInset;
    const Point centre{(box.left + box.right) / 2.f, (box.top + box.bottom) / 2.f};
    return {centre, Point{left, top}, Point{right, top}, Point{left, bottom}, Point{right, bottom}};
}
} // namespace UI::Mobile::TouchTarget
