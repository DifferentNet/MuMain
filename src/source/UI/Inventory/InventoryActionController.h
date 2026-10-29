// NewUIInventoryActionController.h
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIINVENTORYACTIONCONTROLLER_H__INCLUDED_)
#define AFX_NEWUIINVENTORYACTIONCONTROLLER_H__INCLUDED_

#pragma once

#include "UI/Inventory/InventoryCtrl.h"
#include "GameLogic/Items/IInventoryActionContext.h"

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
    // MU Moldova: a finger's long press is a right click; on an item it opens the item's card
    // (UI::Mobile::ItemCard) instead. False when there is no item to show.
    bool ShowTouchItemCard(CInventoryCtrl* targetControl) const;
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