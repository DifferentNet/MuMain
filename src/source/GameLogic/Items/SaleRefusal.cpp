#include "GameLogic/Items/SaleRefusal.h"

namespace GameLogic::Items
{
SaleRefusal CheckSale(std::int64_t zen, std::int64_t value, bool hasHarmonyOption)
{
    if (zen + value > kMaxCarriedZen)
    {
        return SaleRefusal::ZenCap;
    }
    if (hasHarmonyOption)
    {
        return SaleRefusal::ReinforcedItem;
    }
    return SaleRefusal::None;
}
} // namespace GameLogic::Items
