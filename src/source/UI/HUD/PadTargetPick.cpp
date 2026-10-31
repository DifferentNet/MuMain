#include "UI/HUD/PadTargetPick.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace UI::HUD::PadTargetPick
{
namespace
{
// A target this far aside counts as much as this many times as far ahead: the d-pad keeps to a row or a
// column before it turns off it.
constexpr float AsideWeight = 3.f;
// A centre must lie this share of the box's own size past the one it leaves, so a cell's neighbour in
// the next row is not taken for the one beside it.
constexpr float AheadShare = 0.25f;

float Area(const Box& box)
{
    return (box.right - box.left) * (box.bottom - box.top);
}

// How far the point is from the box: 0 inside it.
float DistanceTo(const Box& box, float x, float y)
{
    const float dx = std::max({box.left - x, 0.f, x - box.right});
    const float dy = std::max({box.top - y, 0.f, y - box.bottom});
    return std::hypot(dx, dy);
}

// How far `to` lies the way (dx, dy) points from `from`, weighing a step aside more than one ahead;
// a negative score when it does not lie that way.
float Score(const Box& from, const Box& to, int dx, int dy)
{
    const float ahead = dx != 0 ? (to.CentreX() - from.CentreX()) * static_cast<float>(dx)
                                : (to.CentreY() - from.CentreY()) * static_cast<float>(dy);
    const float size = dx != 0 ? from.right - from.left : from.bottom - from.top;
    if (ahead <= size * AheadShare)
        return -1.f;
    const float aside = dx != 0 ? std::abs(to.CentreY() - from.CentreY()) : std::abs(to.CentreX() - from.CentreX());
    return ahead + aside * AsideWeight;
}

int BestThatWay(std::span<const Target> targets, int from, int dx, int dy, bool sameWindow)
{
    const Target& start = targets[from];
    int best = None;
    float bestScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
    {
        if (i == from || (targets[i].window == start.window) != sameWindow)
            continue;
        const float score = Score(start.box, targets[i].box, dx, dy);
        if (score >= 0.f && score < bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    return best;
}
} // namespace

int At(std::span<const Target> targets, float x, float y)
{
    int found = None;
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
    {
        if (targets[i].box.Contains(x, y) && (found == None || Area(targets[i].box) < Area(targets[found].box)))
            found = i;
    }
    return found;
}

int Nearest(std::span<const Target> targets, float x, float y, float reach)
{
    int found = None;
    float nearest = reach;
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
    {
        const float distance = DistanceTo(targets[i].box, x, y);
        if (distance <= nearest)
        {
            nearest = distance;
            found = i;
        }
    }
    return found;
}

int NextThatWay(std::span<const Target> targets, int from, int dx, int dy)
{
    if (from < 0 || from >= static_cast<int>(targets.size()) || (dx == 0 && dy == 0))
        return None;
    const int inWindow = BestThatWay(targets, from, dx, dy, true);
    return inWindow != None ? inWindow : BestThatWay(targets, from, dx, dy, false);
}
} // namespace UI::HUD::PadTargetPick
