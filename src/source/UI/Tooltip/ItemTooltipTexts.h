#pragma once

#include <string_view>

#include "GameLogic/Items/ItemRarity.h"
#include "GameLogic/Items/LootFilter.h"

// The rich item tooltip's fixed texts in the current language (ItemTooltipLayout.h's RichInput). Hovering an item asks
// for them every frame, so every one is built once per language and kept: the views stay valid until the next call
// after the language changes, so take them for the frame, never keep them.
namespace UI::Tooltip
{
// The line under the item's name: "Ancient · Armour", "Excellent · Weapons", "Wings"; the rarity prefix for ancient
// and excellent items only. Empty for zen and other items, which have no kind line.
[[nodiscard]] std::string_view ItemKindText(GameLogic::Items::LootFilter::Category category,
                                            GameLogic::Items::Rarity::Tier rarity);

// "Hold Shift for details", under a short view.
[[nodiscard]] std::string_view ShiftForDetailsHint();
} // namespace UI::Tooltip
