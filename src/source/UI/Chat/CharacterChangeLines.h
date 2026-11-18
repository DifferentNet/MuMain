#pragma once

#include <cstdint>

typedef struct tagITEM ITEM;

// Chat lines for changes the server makes to the hero without a word of its own: what a jewel did to an item, what a
// death took, what a sale brought and a repair cost, and the stat points left to spend.
namespace UI::Chat::ChangeLines
{
// A Jewel of Bless, Soul or Life was used on `before`, which is now `after` (the same item kind). No line when
// nothing comparable changed.
void PostJewelUpgrade(const ITEM& before, const ITEM& after);

// The carried zen changed by `delta` (a money update). Whether a pending sale, repair or death counted it, so the
// caller says nothing of it itself.
bool NoteZenChange(std::int64_t delta);

// An item sale was asked of the server, or the server refused it.
void NoteSaleAsked();
void NoteSaleRefused();
// "Repair all" was asked (the caller checked that something is damaged), or the repair of one item: nothing is
// waited for when that item has its full durability (the server takes nothing for it).
void NoteRepairAsked();
void NoteRepairAsked(const ITEM& item);

// The hero died. Outside duels and event maps (where nothing is lost) the zen and experience lost are said.
void NoteHeroDeath();

// The hero came back after a death: the experience before and after the respawn packet.
void NoteRevivalExperience(std::int64_t before, std::int64_t after);

// A level-up (at most one line a minute) or a reset (always): the stat points to spend, when there are any.
void NoteLevelUp(int level, int freePoints, bool reset);

// Once a frame: says a sum that has settled.
void Tick();
} // namespace UI::Chat::ChangeLines
