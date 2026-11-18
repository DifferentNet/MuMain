#pragma once

// Chat lines about the hero's state that nothing said before: a buff of the hero that ended, a bag that is almost
// full, a worn item that broke. The potion and arrow lines live where those counts are (UI/HUD/ItemHotKey.cpp,
// Engine/Object/ZzzInterface.cpp).
namespace UI::Chat::StatusLines
{
// A buff (eBuffState) of the hero ended or started; ends are said a moment later (UI/Chat/BuffEndNotices.h).
void NoteHeroBuffEnded(int buff);
void NoteHeroBuffStarted(int buff);

// An item the hero picked up went into the bag (not zen, not a stack that grew).
void NoteItemPickedUp();

// The server set a worn item's durability (slot below MAX_EQUIPMENT) from `before` to `after`.
void NoteEquipmentDurability(int slot, int before, int after);

// Once a frame: says the buff ends that settled.
void Tick();
} // namespace UI::Chat::StatusLines
