// NewUIInventoryActionController.h
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIINVENTORYACTIONCONTROLLER_H__INCLUDED_)
#define AFX_NEWUIINVENTORYACTIONCONTROLLER_H__INCLUDED_

#pragma once

#include "UI/Inventory/InventoryCtrl.h"
#include "GameLogic/Items/IInventoryActionContext.h"

#include <functional>

namespace UI::Mobile::ItemCard
{
struct Content;
}

namespace mu::ui::window
{

class CInventoryActionController
{
public:
    CInventoryActionController();
    ~CInventoryActionController() = default;

    void SetContext(SEASON3B::IInventoryActionContext* pContext);
    bool HandleInventoryActions(CInventoryCtrl* targetControl) const;

private:
    bool HandlePickedItemPlacement(CInventoryCtrl* targetControl) const;
    bool TryApplyJewel(CInventoryCtrl* targetControl, CPickedItem* pPickedItem, ITEM* pPickItem,
                       int iSourceIndex, int iTargetIndex) const;
    bool TryStackItem(CInventoryCtrl* targetControl, ITEM* pPickItem, int iSourceIndex, int iTargetIndex) const;
    bool TryMoveItem(CInventoryCtrl* targetControl, CPickedItem* pPickedItem, ITEM* pPickItem,
                     int iSourceIndex, int iTargetIndex) const;

    bool HandleRepairClick(CInventoryCtrl* targetControl) const;

    // What a right click on a bag item does, by the windows open with the bag.
    enum class RightClickTarget
    {
        None,
        Storage,
        Shop,
        Mix,
        Trade,
        Inventory,
    };
    RightClickTarget FindRightClickTarget() const;

    bool HandleRightClick(CInventoryCtrl* targetControl) const;
    // The right click's action on the item under the pointer; without mayDrop an item with no other
    // use stays where it is.
    bool RunRightClickAction(CInventoryCtrl* targetControl, RightClickTarget target, bool mayDrop) const;
    // MU Moldova: whether this right click opens the item's card (UI::Mobile::ItemCard) instead of
    // acting: a finger's long press always does; the pad's R3 too, but for the storage and the chaos
    // machine, where it moves the item at once as the mouse does, to shift many in a row.
    static bool AsksWithItemCard(RightClickTarget target);
    // The item card for the item under the pointer. False when there is no item to show.
    bool ShowTouchItemCard(CInventoryCtrl* targetControl) const;
    // The card's first button with the bag alone open: Use or Equip as the right click does, Apply...
    // for a jewel (lifts it, to be tapped onto an item), Open for a box the server opens when dropped.
    // None for an item the right click would only drop: the card's Drop asks first.
    void SetBagItemAction(CInventoryCtrl* targetControl, const ITEM& item, std::function<void()> runRightClick,
                          UI::Mobile::ItemCard::Content& card) const;
    bool HandleStorageAutoMove(CInventoryCtrl* targetControl) const;
    bool HandleMixAutoMove(CInventoryCtrl* targetControl) const;
    bool HandleSellToNPC(CInventoryCtrl* targetControl) const;
    bool HandleInventoryRightClickActions(CInventoryCtrl* targetControl, bool mayDrop) const;
    bool TryEquipItem(CInventoryCtrl* targetControl, ITEM* pItem, int iSrcIndex) const;
    bool TryDropItem(CInventoryCtrl* targetControl, ITEM* pItem) const;

    int FindAlternateEquipSlot(int nOriginalSlot, ITEM* pItem) const;
    bool IsSlotOccupied(int nSlot) const;

    bool ApplyJewels(CInventoryCtrl* targetControl, CPickedItem* pPickedItem, ITEM* pPickItem,
                     int iSourceIndex, int iTargetIndex) const;
    bool TryStackItems(CInventoryCtrl* targetControl, ITEM* pPickItem, int iSourceIndex, int iTargetIndex) const;
    bool RepairItemAtMousePoint(CInventoryCtrl* targetControl) const;
    bool TryConsumeItem(CInventoryCtrl* targetControl, ITEM* pItem, int iIndex) const;
    bool TryTransferBetweenInventorySections(CInventoryCtrl* sourceControl) const;

    SEASON3B::IInventoryActionContext* m_pContext;
};

} // namespace mu::ui::window

#endif // !defined(AFX_NEWUIINVENTORYACTIONCONTROLLER_H__INCLUDED_)