#pragma once

#include <cstdint>
#include <memory>
#include <span>

typedef struct tagITEM ITEM;

// An item a window shows but the inventory doesn't hold (a lot of the auction, a GM panel's look at a character's
// bag): read from the bytes the server sends for an item, as the item manager reads them, but owned by the caller,
// with no key and outside the manager's list.
namespace UI::Items::FromData
{
// The item those bytes describe (ParseItemData); null when there are too few of them for an item.
std::unique_ptr<ITEM> Make(std::span<const std::uint8_t> data);
} // namespace UI::Items::FromData
