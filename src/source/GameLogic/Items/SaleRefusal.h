#pragma once

#include <cstdint>

typedef struct tagITEM ITEM;

// Why an NPC shop would refuse to buy an item, checked before the sale is sent: the server refuses these sales
// without a word, so both ways of selling (dropping the item on the shop, right-click) say it in the chat instead
// (UI/NPCs/NPCShop.cpp, UI/Inventory/InventoryActionController.cpp).
namespace GameLogic::Items
{
// The most zen a character can carry; a sale that would pass it is refused.
inline constexpr std::int64_t kMaxCarriedZen = 2'000'000'000;

enum class SaleRefusal
{
    None,
    ZenCap,         // the price would take the zen past kMaxCarriedZen
    ReinforcedItem, // a Jewel of Harmony option is on the item
};

SaleRefusal CheckSale(std::int64_t zen, std::int64_t value, bool hasHarmonyOption);

// A refusal's chat line: the zen cap is told as a system line, a reinforced item as an error, as dropping the item
// on the shop always told them. Empty (false) when the item sells.
struct SaleRefusalLine
{
    const wchar_t* text = nullptr;
    bool asError = false;

    explicit operator bool() const { return text != nullptr; }
};

// The chat line for item's refusal when zen is carried and the shop pays value for it.
SaleRefusalLine SaleRefusalText(const ITEM* item, std::int64_t zen, std::int64_t value);
} // namespace GameLogic::Items
