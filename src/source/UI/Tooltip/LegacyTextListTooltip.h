#pragma once

namespace UI::Tooltip
{
enum class Placement
{
    Below,
    Above,
};

// The item information tooltip's owner in the shared tooltip (RenderItemInfo(), RenderRepairInfo(),
// the pet's): the default below, so a window that stops showing an item hides that and nothing else.
const void* ItemInfoOwner();

// Presents the current TextList/TextListColor/TextBold contents as a hover tooltip.
// The anchor is already in screen pixels; callers convert from their own coordinate space.
void ShowLegacyTextList(int lineCount, float anchorX, float anchorY,
                        Placement placement = Placement::Below, const void* owner = ItemInfoOwner());
void HideLegacyTextList(const void* owner = ItemInfoOwner());

// Where an item tooltip goes above a row instead of its window's usual place (an item's name in the chat log), in
// screen pixels: centred on x with its bottom at bottomY; when it does not fit above, its left edge at besideX
// (right of the window the row is in) instead.
struct AboveRow
{
    float x = 0.f;
    float bottomY = 0.f;
    float besideX = 0.f;
};
void ShowLegacyTextListAboveRow(int lineCount, const AboveRow& spot, const void* owner = ItemInfoOwner());
}
