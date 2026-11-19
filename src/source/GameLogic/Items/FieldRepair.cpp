#include "stdafx.h"
#include "GameLogic/Items/FieldRepair.h"

#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/ShopRestrictions.h"
#include "Network/Server/WSclient.h"
#include "UI/Chat/CharacterChangeLines.h"

namespace GameLogic::Items::FieldRepair
{
namespace
{
// A worn item that lost durability and may be repaired.
bool NeedsRepair(ITEM* item)
{
    if (item == nullptr || item->Type == -1 || IsRepairBan(item))
        return false;
    return item->Durability < CalcMaxDurability(item, &ItemAttribute[item->Type], item->Level);
}

// The self-repair price of such an item, as the helper and the bag's tooltip count it.
std::int64_t SelfRepairCostOf(ITEM* item)
{
    const int maxDurability = CalcMaxDurability(item, &ItemAttribute[item->Type], item->Level);
    return CalcSelfRepairCost(ItemValue(item, 2), item->Durability, maxDurability, item->Type);
}
} // namespace

Plan PlanRepairAll(std::int64_t zen)
{
    static_assert(MaxSlots >= MAX_EQUIPMENT);

    Plan plan;
    for (int slot = 0; slot < MAX_EQUIPMENT; ++slot)
    {
        ITEM* item = &CharacterMachine->Equipment[slot];
        if (slot == EQUIPMENT_HELPER || !NeedsRepair(item))
            continue;

        const std::int64_t cost = SelfRepairCostOf(item);
        if (plan.cost + cost > zen)
        {
            plan.shortOfZen = true;
            continue;
        }
        plan.cost += cost;
        plan.slots[static_cast<size_t>(plan.count++)] = slot;
    }
    return plan;
}

void Send(const Plan& plan)
{
    if (plan.count <= 0 || SocketClient == nullptr)
        return;

    UI::Chat::ChangeLines::NoteRepairAsked();
    for (int i = 0; i < plan.count; ++i)
        SocketClient->ToGameServer()->SendRepairItemRequest(plan.slots[static_cast<size_t>(i)], 1);
}
} // namespace GameLogic::Items::FieldRepair
