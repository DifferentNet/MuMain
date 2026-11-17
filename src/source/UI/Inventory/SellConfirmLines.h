#pragma once

#include "UI/Dialogs/GenericConfirmDialog.h"

#include <vector>

// The lines the high-value sell confirmation adds under its warning (UI/NPCs/NPCShop.cpp,
// UI/Inventory/InventoryActionController.cpp): the item's name and the price the NPC pays, the same price
// the item tooltip shows.
namespace UI::Inventory
{
// No lines for a missing item.
std::vector<mu::ui::window::GenericDialogConfig::Line> SellPriceLines(ITEM* item);
} // namespace UI::Inventory
