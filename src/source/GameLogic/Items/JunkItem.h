#pragma once

#include "GameLogic/Items/JunkMarks.h"

typedef struct tagITEM ITEM;

// Reads the junk rule's facts from the game's item (JunkMarks.h holds the rule itself).
namespace GameLogic::Items::JunkMarks
{
Traits ReadTraits(const ITEM& item);
Fingerprint FingerprintOf(const ITEM& item);
// CanBeJunk for the item: plain weapons, shields and armour only.
bool MayBeJunk(const ITEM& item);
} // namespace GameLogic::Items::JunkMarks
