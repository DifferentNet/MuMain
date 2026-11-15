#pragma once

namespace GameLogic::Items
{
// How worn an item is, by its durability against the most it can have. The original coloured the durability
// warning icons and their tooltip by it (UI/Inventory/ItemEnduranceInfo.cpp); item tooltips colour the
// "Durability" line by it too (Engine/Object/ZzzInventory.cpp).
enum class DurabilityBand
{
    Fine,  // above half
    Half,  // at most half
    Third, // at most 30 %
    Fifth, // at most 20 %
    Zero,  // broken
};

[[nodiscard]] DurabilityBand ClassifyDurability(int durability, int maxDurability);

// The band's class suffix in item_endurance.rcss ("half", "third", "fifth", "zero"); "" for Fine.
[[nodiscard]] const char* DurabilityBandName(DurabilityBand band);
} // namespace GameLogic::Items
