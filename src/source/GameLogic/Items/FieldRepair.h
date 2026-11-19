#pragma once

#include <array>
#include <cstdint>

// "Repair all" away from a shop: the bag's second hammer in repair mode, or A (UI/Inventory/MyInventory.cpp). The
// same self-repair request as a click on each worn item, one per item, at the self-repair price.
namespace GameLogic::Items::FieldRepair
{
// Up to one slot per worn item.
constexpr int MaxSlots = 12;

// Above this sum the bag asks once before repairing (self repair costs more than the NPC's).
constexpr std::int64_t ConfirmAboveZen = 1'000'000;

struct Plan
{
    std::array<int, MaxSlots> slots{};
    int count = 0;
    std::int64_t cost = 0;   // the sum of the planned repairs
    bool shortOfZen = false; // a damaged item was left out because the zen ran out
};

// The worn items that lost durability and may be repaired, in slot order, as long as the zen covers them. The pet
// slot is left out as at the shop's "Repair all" (the pet trainer repairs pets).
[[nodiscard]] Plan PlanRepairAll(std::int64_t zen);

// Sends the planned requests; the zen spent comes as one chat line (UI::Chat::ChangeLines::NoteRepairAsked).
void Send(const Plan& plan);
} // namespace GameLogic::Items::FieldRepair
