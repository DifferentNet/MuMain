#include "stdafx.h"
#include "GameLogic/Items/SaleRefusal.h"

#include "I18N/All.h"

namespace GameLogic::Items
{
SaleRefusalLine SaleRefusalText(const ITEM* item, std::int64_t zen, std::int64_t value)
{
    if (item == nullptr)
    {
        return {};
    }

    switch (CheckSale(zen, value, item->Jewel_Of_Harmony_Option != 0))
    {
    case SaleRefusal::ZenCap:
        return {I18N::Game::ExceededMaximumAmountOfZenYouCanPossess, false};
    case SaleRefusal::ReinforcedItem:
        return {I18N::Game::ReinforcedItemCanTBeSold, true};
    case SaleRefusal::None:
        break;
    }
    return {};
}
} // namespace GameLogic::Items
