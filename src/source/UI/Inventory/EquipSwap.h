#pragma once

typedef struct tagITEM ITEM;

// Putting a bag item on in place of a worn one (pad round 6, N1; ghinzuro 2026-11-18), for the pad's X and the touch
// card's Equip in the bag and for the quick equip offer (UI/Inventory/QuickEquip). The server can't swap an item
// into a taken place, so the swap is two ordinary item moves, each waiting for the server's answer: first the worn
// item goes to a free bag cell, then the new item goes on. Nothing is ever on the cursor between them, so an item
// can't be lost: a refused first move leaves everything as it was, a refused second one leaves both items in the bag.
namespace UI::Inventory::EquipSwap
{
// Why an item can't go on now (Plan::why).
enum class Refusal
{
    None,
    CannotWear,  // class, level, stats or the place itself
    TwoItemsOff, // a two-handed weapon, a bow or what holds one: it would take two worn items off
    NoRoom,      // no free bag cell for the worn item
    FlightGear,  // Icarus without wings or a flying mount
};

// Where a bag item would go now: a free place (swap false), or the place of the one worn item it replaces (swap
// true); slot -1 and why when it can't. Says nothing.
struct Plan
{
    int slot = -1;
    bool swap = false;
    Refusal why = Refusal::CannotWear;
};

Plan PlanFor(const ITEM& item);

// Whether items may move between the bag and the gear now: no item on the cursor or waiting for the server, no
// swap under way, no trade, shop, vault, chaos machine, personal store or other window that takes bag items, no
// repair, the hero alive.
bool CanMoveGearNow();

// Puts the bag item at bagIndex (the protocol's slot) on, taking the worn item off first when its place is taken;
// says in the chat why not. True when a move was sent or a refusal was said.
bool PutOn(int bagIndex);

// The answer to an item move (WSclient's ReceiveEquipmentItemExtended): the next step, or the end of the swap.
void OnMoveAnswer(bool accepted);
// A move given up on, or the whole inventory sent again: the swap stops.
void Stop();
// Once a frame: sends the second move a frame after the first one's answer.
void Tick();
} // namespace UI::Inventory::EquipSwap
