#pragma once

#include "GameLogic/Items/ItemRarity.h"
#include "UI/Tooltip/ItemTooltipLayout.h"

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

// What an item information tooltip shows besides its lines, for a theme that lays item tooltips out richly.
struct ItemPresentation
{
    GameLogic::Items::Rarity::Tier rarity = GameLogic::Items::Rarity::Tier::Normal;
    GameLogic::Items::Rarity::UpgradeBand band = GameLogic::Items::Rarity::UpgradeBand::None;
    RichInput layout;
};

// Whether item information tooltips are laid out richly now: the theme declares RichItemTooltip=1 and the view is
// not the phone one. Only then is an ItemPresentation worth building.
bool UsesRichItemLayout();

// Presents the current TextList/TextListColor/TextBold contents as a hover tooltip.
// The anchor is already in screen pixels; callers convert from their own coordinate space.
// With `presentation` (item information only) and UsesRichItemLayout(), the lines get the rich layout, their roles
// read from TextListRole.
void ShowLegacyTextList(int lineCount, float anchorX, float anchorY, Placement placement = Placement::Below,
                        const void* owner = ItemInfoOwner(), const ItemPresentation* presentation = nullptr);
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
void ShowLegacyTextListAboveRow(int lineCount, const AboveRow& spot, const void* owner = ItemInfoOwner(),
                                const ItemPresentation* presentation = nullptr);
}
