#include <doctest.h>

#include "GameLogic/Items/SaleRefusal.h"

using GameLogic::Items::CheckSale;
using GameLogic::Items::kMaxCarriedZen;
using GameLogic::Items::SaleRefusal;

TEST_CASE("A sale up to the zen cap goes through")
{
    CHECK(CheckSale(0, 1000, false) == SaleRefusal::None);
    CHECK(CheckSale(kMaxCarriedZen - 1000, 1000, false) == SaleRefusal::None);
}

TEST_CASE("A sale past the zen cap is refused")
{
    CHECK(CheckSale(kMaxCarriedZen - 999, 1000, false) == SaleRefusal::ZenCap);
    CHECK(CheckSale(kMaxCarriedZen, 1, false) == SaleRefusal::ZenCap);
}

TEST_CASE("A Jewel of Harmony option refuses the sale, the zen cap is told first")
{
    CHECK(CheckSale(0, 1000, true) == SaleRefusal::ReinforcedItem);
    CHECK(CheckSale(kMaxCarriedZen, 1000, true) == SaleRefusal::ZenCap);
}
