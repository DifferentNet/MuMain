#include "stdafx.h"

#include "UI/Inventory/EquipSwap.h"

#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/EquipmentRestrictions.h"
#include "GameLogic/Items/InventoryUtils.h"
#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/ItemCombatPower.h"
#include "I18N/All.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryContents.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/InventorySort.h"
#include "UI/Inventory/MyInventory.h"
#include "World/MapInfra/MapManager.h"

#include <array>

namespace
{
using mu::ui::window::CInventoryCtrl;
using mu::ui::window::CPickedItem;

enum class Phase
{
    Idle,
    TakingOff, // the worn item's move to the bag waits for its answer
    Ready,     // it is in the bag: the new item goes on next frame
    PuttingOn, // the new item's move waits for its answer
};

struct Swap
{
    Phase phase = Phase::Idle;
    int bagIndex = -1;
    DWORD itemKey = 0;
    int slot = -1;
};

Swap g_swap;

// The windows that take bag items or sell them: nothing moves to the gear behind them.
constexpr std::array BlockingWindows = {
    mu::ui::window::INTERFACE_NPCSHOP,          mu::ui::window::INTERFACE_TRADE,
    mu::ui::window::INTERFACE_STORAGE,          mu::ui::window::INTERFACE_STORAGE_EXT,
    mu::ui::window::INTERFACE_MIXINVENTORY,     mu::ui::window::INTERFACE_MYSHOP_INVENTORY,
    mu::ui::window::INTERFACE_PURCHASESHOP_INVENTORY, mu::ui::window::INTERFACE_INVENTORY_EXT,
    mu::ui::window::INTERFACE_LUCKYITEMWND,
};

void SayError(const wchar_t* text)
{
    // Repeated presses give one line.
    if (!g_pSystemLogBox->CheckChatRedundancy(text))
        g_pSystemLogBox->AddText(text, mu::ui::window::TYPE_ERROR_MESSAGE);
}

bool IsEmpty(int slot)
{
    return CharacterMachine->Equipment[slot].Type == -1;
}

bool IsBowOrCrossbow(const ITEM& item)
{
    return GameLogic::Items::IsBowType(item.Type) || GameLogic::Items::IsCrossbowType(item.Type);
}

bool IsTwoHanded(const ITEM& item)
{
    return item.Type >= 0 && item.Type < MAX_ITEM && ItemAttribute[item.Type].TwoHand;
}

// The other place a one-handed weapon (the classes that hold two) or a ring fits, as the right click finds it.
int AlternateSlot(int slot, const ITEM& item)
{
    if (slot == EQUIPMENT_RING_RIGHT)
        return EQUIPMENT_RING_LEFT;
    if (slot != EQUIPMENT_WEAPON_RIGHT || IsTwoHanded(item))
        return -1;
    const auto baseClass = gCharacterManager.GetBaseClass(Hero->Class);
    const bool holdsTwo = baseClass == CLASS_KNIGHT || baseClass == CLASS_DARK || baseClass == CLASS_RAGEFIGHTER;
    return holdsTwo ? EQUIPMENT_WEAPON_LEFT : -1;
}

// Putting item into a weapon hand would take the other hand's item off too: a two-handed weapon or a bow next to
// anything but its ammunition, or anything next to one. v1 swaps one worn item only.
bool TakesOtherHandOff(int slot, const ITEM& item)
{
    if (slot != EQUIPMENT_WEAPON_RIGHT && slot != EQUIPMENT_WEAPON_LEFT)
        return false;
    const int otherSlot = slot == EQUIPMENT_WEAPON_RIGHT ? EQUIPMENT_WEAPON_LEFT : EQUIPMENT_WEAPON_RIGHT;
    const ITEM& other = CharacterMachine->Equipment[otherSlot];
    if (other.Type == -1)
        return false;
    const bool itemIsAmmo = GameLogic::Items::IsAmmunition(&item);
    const bool otherIsAmmo = GameLogic::Items::IsAmmunition(&other);
    if (IsBowOrCrossbow(item))
        return !otherIsAmmo;
    if (IsBowOrCrossbow(other))
        return !itemIsAmmo;
    return (IsTwoHanded(item) && !itemIsAmmo) || (IsTwoHanded(other) && !otherIsAmmo);
}

int WornCombatPower(int slot)
{
    const ITEM& worn = CharacterMachine->Equipment[slot];
    return GameLogic::Items::GetItemCombatPower(GameLogic::Items::DescribeForCombatPower(worn));
}

bool CanTakeOffSilently(int slot)
{
    return GameLogic::Items::CanTakeOff(slot, gMapManager.WorldActive, &CharacterMachine->Equipment[EQUIPMENT_HELPER],
                                        &CharacterMachine->Equipment[EQUIPMENT_WING]);
}

// The worn item a swap replaces among the places item fits: of two, the one worth less Combat Power.
int SlotToReplace(const std::array<int, 2>& places, int count)
{
    if (count == 1)
        return places[0];
    return WornCombatPower(places[1]) < WornCombatPower(places[0]) ? places[1] : places[0];
}

// The grid of the bag page that holds the item at index.
CInventoryCtrl* GridHolding(int index)
{
    for (int g = 0; g < mu::ui::window::CMyInventory::OwnedBagGridCount; ++g)
    {
        CInventoryCtrl* grid = g_pMyInventory->OwnedBagGrid(g);
        if (grid != nullptr && grid->FindItem(index) != nullptr)
            return grid;
    }
    return nullptr;
}

// Sends the bag item's move onto the free place slot, as the right click does.
bool SendPutOn(int bagIndex, int slot)
{
    CInventoryCtrl* grid = GridHolding(bagIndex);
    ITEM* item = grid != nullptr ? grid->FindItem(bagIndex) : nullptr;
    // Owned by its grid, so a refused move puts it back in its cell.
    if (item == nullptr || !CInventoryCtrl::CreatePickedItem(grid, item))
        return false;
    CPickedItem* picked = CInventoryCtrl::GetPickedItem();
    if (picked == nullptr)
        return false;
    grid->RemoveItem(item);
    picked->HidePickedItem();
    return SendRequestEquipmentItem(STORAGE_TYPE::INVENTORY, bagIndex, picked->GetItem(), STORAGE_TYPE::INVENTORY,
                                    slot);
}

void SayNoRoom(int slot)
{
    const ITEM& worn = CharacterMachine->Equipment[slot];
    wchar_t name[128]{};
    GetItemName(worn.Type, worn.Level, name);
    wchar_t text[256]{};
    mu_swprintf(text, I18N::Game::EquipNoRoomS, name);
    SayError(text);
}

void SayRefusal(const UI::Inventory::EquipSwap::Plan& plan)
{
    using UI::Inventory::EquipSwap::Refusal;
    switch (plan.why)
    {
    case Refusal::TwoItemsOff:
        SayError(I18N::Game::EquipSwapTwoItems);
        break;
    case Refusal::NoRoom:
        SayNoRoom(plan.slot);
        break;
    case Refusal::FlightGear:
        SayError(I18N::Game::KeepFlightEquipmentInIcarus);
        break;
    default:
        SayError(I18N::Game::PadCannotEquip);
        break;
    }
}

void Finish()
{
    g_swap = {};
}

bool IsBlockingWindowOpen()
{
    for (const auto window : BlockingWindows)
    {
        if (g_pNewUISystem->IsVisible(window))
            return true;
    }
    return false;
}

// The hero may move gear: alive, nothing on the cursor or waiting for the server, no window that takes bag items.
bool IsGearFree()
{
    if (Hero == nullptr || Hero->Dead != 0)
        return false;
    if (IsItemMovePending() || CInventoryCtrl::GetPickedItem() != nullptr || UI::Inventory::Sort().IsWaiting())
        return false;
    return g_pMyInventory->GetRepairMode() != SEASON3B::REPAIR_MODE_ON && !IsBlockingWindowOpen();
}

// The bag item the swap puts on is still in its cell, its place is free, and the hero may still wear it: the
// stats can drop when the worn item comes off (a set bonus).
bool CanSendSecondMove()
{
    if (!IsGearFree() || !IsEmpty(g_swap.slot))
        return false;
    ITEM* item = UI::Inventory::FindPlayerItem(g_swap.bagIndex);
    return item != nullptr && item->Key == g_swap.itemKey && g_pMyInventory->IsEquipable(g_swap.slot, item);
}
} // namespace

namespace UI::Inventory::EquipSwap
{
Plan PlanFor(const ITEM& item)
{
    Plan plan;
    if (Hero == nullptr || CharacterMachine == nullptr || item.Type < 0 || item.Type >= MAX_ITEM)
        return plan;
    const int baseSlot = ItemAttribute[item.Type].m_byItemSlot;
    if (baseSlot < 0 || baseSlot >= MAX_EQUIPMENT_INDEX)
        return plan;

    // The places it may go, a free one first.
    std::array<int, 2> places{};
    int count = 0;
    ITEM* wearable = const_cast<ITEM*>(&item);
    for (const int slot : {baseSlot, AlternateSlot(baseSlot, item)})
    {
        if (slot >= 0 && g_pMyInventory->IsEquipable(slot, wearable))
            places[count++] = slot;
    }
    if (count == 0)
        return plan;

    for (int i = 0; i < count; ++i)
    {
        if (IsEmpty(places[i]))
        {
            plan.slot = places[i];
            plan.why = TakesOtherHandOff(places[i], item) ? Refusal::TwoItemsOff : Refusal::None;
            if (plan.why != Refusal::None)
                plan.slot = -1;
            return plan;
        }
    }

    const int slot = SlotToReplace(places, count);
    const ITEM& worn = CharacterMachine->Equipment[slot];
    // A refusal keeps the place, for its line.
    plan.slot = slot;
    if (TakesOtherHandOff(slot, item))
        plan.why = Refusal::TwoItemsOff;
    else if (!CanTakeOffSilently(slot))
        plan.why = Refusal::FlightGear;
    else if (g_pMyInventory->FindEmptySlot(const_cast<ITEM*>(&worn)) == -1)
        plan.why = Refusal::NoRoom;
    else
        plan.why = Refusal::None;
    plan.swap = true;
    return plan;
}

bool CanMoveGearNow()
{
    return g_swap.phase == Phase::Idle && IsGearFree();
}

bool PutOn(int bagIndex)
{
    if (!IsPlayerInventorySlot(bagIndex) || !CanMoveGearNow())
        return false;
    const ITEM* item = FindPlayerItem(bagIndex);
    if (item == nullptr)
        return false;
    const Plan plan = PlanFor(*item);
    if (plan.why != Refusal::None)
    {
        SayRefusal(plan);
        return true;
    }
    if (!plan.swap)
        return SendPutOn(bagIndex, plan.slot);

    const DWORD itemKey = item->Key;
    if (!g_pMyInventory->TakeOffToBag(plan.slot, true))
        return true;
    g_swap = {Phase::TakingOff, bagIndex, itemKey, plan.slot};
    return true;
}

void OnMoveAnswer(bool accepted)
{
    switch (g_swap.phase)
    {
    case Phase::TakingOff:
        // Refused: the worn item went back on, nothing else moved.
        if (accepted)
            g_swap.phase = Phase::Ready;
        else
            Finish();
        break;
    case Phase::PuttingOn:
        if (!accepted)
            SayError(I18N::Game::EquipSwapStopped);
        Finish();
        break;
    default:
        break;
    }
}

void Stop()
{
    Finish();
}

void Tick()
{
    if (g_swap.phase == Phase::TakingOff || g_swap.phase == Phase::PuttingOn)
    {
        // A move the client gave up on: the inventory list sorts the items out.
        if (!IsItemMovePending())
            Finish();
        return;
    }
    if (g_swap.phase != Phase::Ready)
        return;
    if (!CanSendSecondMove() || !SendPutOn(g_swap.bagIndex, g_swap.slot))
    {
        // Both items are in the bag: nothing is lost, the player puts it on again.
        SayError(I18N::Game::EquipSwapStopped);
        Finish();
        return;
    }
    g_swap.phase = Phase::PuttingOn;
}
} // namespace UI::Inventory::EquipSwap
