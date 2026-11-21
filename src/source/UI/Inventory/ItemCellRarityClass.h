#pragma once

#include "GameLogic/Items/ItemRarity.h"

// The classes an item's inventory cells and its worn slot carry for its rarity, in a theme that declares
// ItemRarityCells=1 (theme.ini; RmlTheme.h ThemeHasItemRarityCells). Only such a theme gets them, so another theme's
// markup stays as it was. The theme's item_grid.rcss and my_inventory.rcss draw them: the rarity tints the cells, and
// a frame runs round the item's outer edge, gold at +13..+15, else the rarity's colour, else thin yellow at +7..+12.
// Every value is a string literal starting with a space (or empty), to append to the cell's own classes.
namespace UI::Inventory
{
// " rar-exc", " rar-anc" or " rar-socket" for the rarity that tints (other tiers none), " rf" when the item has a
// frame, then " up-fine" (+7..+12) or " up-gold" (+13..+15). "" for an item with neither.
[[nodiscard]] const char* RarityCellClasses(GameLogic::Items::Rarity::Tier tier,
                                            GameLogic::Items::Rarity::UpgradeBand band);

// The item's outer edges a cell lies on, which its frame draws: " re-t", " re-b", " re-l", " re-r" in that order.
// "" for a cell inside the item.
[[nodiscard]] const char* EdgeClasses(bool top, bool bottom, bool left, bool right);
} // namespace UI::Inventory
