#pragma once

// MU Moldova: the card a finger's long press on an item opens instead of the right click, which
// may drop the item on the ground (docs/touch-controls.md). The inventory decides what the item
// can do here (CInventoryActionController); the card only shows it.

#include <functional>
#include <string>

namespace UI::Mobile::ItemCard
{
// What the card offers for one item. An empty action is left off the card.
struct Content
{
    // The item as it was pressed: its name heads the card, and the drop question shows it.
    ITEM item{};
    // What the right click does with it here (Equip, Use, Sell, Move), and that action.
    std::wstring primaryLabel;
    std::function<void()> primary;
    // Puts it on the potion buttons Q and W.
    std::function<void()> putOnQ;
    std::function<void()> putOnW;
    // Drops it on the ground, after asking.
    std::function<void()> drop;
};

void Show(Content content);
} // namespace UI::Mobile::ItemCard
