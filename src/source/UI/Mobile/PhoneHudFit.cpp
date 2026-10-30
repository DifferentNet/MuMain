#include "UI/Mobile/PhoneHudFit.h"

#include <algorithm>

namespace UI::Mobile::PhoneHudFit
{
namespace
{
// Whether a shown box comes within `gap` of the span [from, to] along one axis.
bool Near(bool shown, float boxFrom, float boxTo, float from, float to, float gap)
{
    return shown && boxTo + gap > from && boxFrom < to + gap;
}

// How many cards fit in one column across [left, right], from the area's top down.
int ColumnRows(const PartyArea& area, float left, float right, float height, float pitch,
               std::span<const Box> controls)
{
    const float bottom = FreeBottom(controls, left, right, area.top, area.gap, area.floor);
    if (bottom - area.top < height)
        return 0;
    return static_cast<int>((bottom - area.top - height) / pitch) + 1;
}

int ColumnsFor(int cards, int rows)
{
    return (cards + rows - 1) / rows;
}

// Whether `cards` cards at `scale`, `rows` to a column, stay clear of the controls and in the area.
bool Fits(const PartyArea& area, int cards, int rows, float scale, std::span<const Box> controls)
{
    const float width = area.cardWidth * scale;
    const float step = width + area.columnGap * scale;
    int left = cards;
    for (int column = 0; left > 0; ++column)
    {
        const float right = area.right - static_cast<float>(column) * step;
        if (right - width < area.left)
            return false;
        const int inColumn = std::min(rows, left);
        if (ColumnRows(area, right - width, right, area.cardHeight * scale, area.cardPitch * scale, controls) < inColumn)
            return false;
        left -= inColumn;
    }
    return true;
}
} // namespace

float FreeBottom(std::span<const Box> boxes, float left, float right, float top, float gap, float floor)
{
    float bottom = floor;
    for (const Box& box : boxes)
    {
        // Boxes above the top are no limit; one reaching past it leaves no room.
        if (Near(box.shown, box.left, box.right, left, right, gap) && box.bottom > top)
            bottom = std::min(bottom, box.top - gap);
    }
    return bottom;
}

PartyFit FitParty(const PartyArea& area, int cards, std::span<const Box> controls)
{
    if (cards <= 0 || area.cardPitch <= 0.f)
        return {};
    const int fewestRows = ColumnsFor(cards, MaxColumns);
    for (int step = 0; step <= CardScaleSteps; ++step)
    {
        const float scale = MaxCardScale - CardScaleStep * static_cast<float>(step);
        for (int rows = cards; rows >= fewestRows; --rows)
        {
            if (Fits(area, cards, rows, scale, controls))
                return {scale, rows, ColumnsFor(cards, rows), true};
        }
    }
    const float smallest = MaxCardScale - CardScaleStep * static_cast<float>(CardScaleSteps);
    return {smallest, fewestRows, ColumnsFor(cards, fewestRows), false};
}

float ShiftLeft(std::span<const Box> boxes, float left, float right, float top, float bottom, float gap, float minLeft)
{
    float rightLimit = right;
    float leftLimit = minLeft;
    for (const Box& box : boxes)
    {
        if (!Near(box.shown, box.top, box.bottom, top, bottom, gap))
            continue;
        if (box.right <= left)
            leftLimit = std::max(leftLimit, box.right + gap);
        else
            rightLimit = std::min(rightLimit, box.left - gap);
    }
    return std::min(0.f, std::max(rightLimit - right, leftLimit - left));
}

bool Overlaps(std::span<const Box> zones, const Box& box)
{
    return std::ranges::any_of(zones, [&box](const Box& zone)
    {
        return zone.shown && zone.left < box.right && box.left < zone.right && zone.top < box.bottom &&
               box.top < zone.bottom;
    });
}
} // namespace UI::Mobile::PhoneHudFit
