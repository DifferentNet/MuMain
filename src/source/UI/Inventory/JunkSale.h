#pragma once

#include <cstdint>

typedef struct tagITEM ITEM;

// Junk marks and "Sell junk" (pad round 6, B7; ghinzuro 2026-11-18: pad and touch only for now). The player marks
// plain gear in the bag as junk (the item card's Junk, the bag's "Mark plain gear"); a marked cell shows a coin. At an
// NPC shop "Sell junk (N)" asks once with the count, the total and the names, then sells the items one by one, each
// after the server's answer to the one before. The rule (GameLogic/Items/JunkMarks.h) is checked again right before
// each sale, and a mark keeps its item's fingerprint, so an old mark never sells another item. The marks are kept
// per character in config.ini [Junk].
namespace UI::Inventory::JunkSale
{
// Whether the bag item in slot (the protocol's index) carries a junk mark.
bool IsMarked(int slot, const ITEM& item);
// Whether the item card offers Junk for it at all: weapons, shields and armour (the rule may still refuse it).
bool OffersMark(const ITEM& item);
// Marks the bag item in slot or takes its mark off; says why not when the rule refuses it.
void ToggleMark(int slot);
// Marks every bag item the rule allows (the bag's "Mark plain gear") and says how many.
void MarkPlainGear();

// The marked items a sale would take now, and what the shop pays for them.
int SellableCount();

// The shop's "Sell junk": asks, then sells.
void AskToSell();
// Whether a sale runs (the shop's button waits meanwhile).
bool IsSelling();
// The shop closed, the hero warped or died: the sale stops after the answer it waits for.
void StopSale();
// The server's answer to a sale (WSclient's ReceiveSell).
void OnSaleAnswer(bool sold);

// The whole inventory came (entering the world, a sort, a resync): the marks of this character are read. Only when
// the list answers a bag sort this client asked for do marks follow their items to new cells; any other list only
// drops the marks whose cell holds another item, so an old mark never jumps onto an item the player didn't mark.
void OnBagListed();
// A bag item moved to another bag cell (WSclient's ReceiveEquipmentItemExtended).
void OnItemMoved(int from, int to);
// Once a frame: the next sale, its timeout, and marks whose item left the bag go.
void Tick();
} // namespace UI::Inventory::JunkSale
