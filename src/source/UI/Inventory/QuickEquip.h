#pragma once

// The quick equip offer (pad round 6, B3; ghinzuro 2026-11-18: pad and touch only): a picked-up item that raises the
// hero's Combat Power (the item tooltip's number) and that the hero may wear is offered for a few seconds, "New:
// <name> +N CP". The pad puts it on with the d-pad right held (UI/HUD/GamepadControls, shown in the pad overlay), a
// finger with the offer's chip (UI/HUD/TouchControls). It never puts anything on by itself: it goes when its time
// runs out, its item leaves the bag, the map changes or the hero dies. The rules are GameLogic/Items/EquipOffer.h;
// putting on is UI/Inventory/EquipSwap.
namespace UI::Inventory::QuickEquip
{
// An item was picked up into the bag cell bagIndex (the protocol's slot).
void NoteItemPicked(int bagIndex);
// The offer's line, "New: <name> +N CP"; nullptr when none is up.
const wchar_t* OfferText();
bool HasOffer();
// Puts the offered item on; false when no offer is up.
bool Accept();
// A finger's tap on the offer: Accept(), unless the offer only just appeared (EquipOffer::TapGuardMs), when the tap
// does nothing.
void AcceptTap();
// Once a frame: the offer runs out or goes with its item, the map or the hero's life.
void Tick();
} // namespace UI::Inventory::QuickEquip
