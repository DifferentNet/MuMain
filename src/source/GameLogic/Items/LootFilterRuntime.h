#pragma once

#include "GameLogic/Items/LootFilter.h"

typedef struct tagITEM ITEM;
class OBJECT;

// The loot filter's link to the game: it reads the settings from config.ini, classifies the items on the ground
// (Items[]) when they drop and again whenever the settings change, and answers the label, shine and beam code.
namespace GameLogic::Items::LootFilterRuntime
{
// The current settings (loaded from config.ini on first use).
[[nodiscard]] const LootFilter::Settings& Current();

// Replaces the settings, saves them to config.ini and reclassifies every item on the ground.
void Apply(const LootFilter::Settings& settings);

// Whether the item counts as excellent, as its label colour does: wings and Fenrir keep their options in the
// excellent bits but are not excellent.
[[nodiscard]] bool IsExcellent(const ITEM& item);

// What the rules need to know about one item on the ground: its type, options and the name its label shows.
[[nodiscard]] LootFilter::Facts BuildFacts(OBJECT& object, ITEM& item);

// Classifies the item in Items[index] and stores the verdict there; a fresh highlighted drop plays the drop sound
// when that is on.
void ClassifyDrop(int index, bool isFreshDrop);

// Classifies every live item on the ground again.
void ReclassifyAll();

// The cached verdict of Items[index] (Normal for a bad index).
[[nodiscard]] LootFilter::Verdict VerdictOf(int index);

[[nodiscard]] bool IsHidden(int index);
[[nodiscard]] bool IsHighlighted(int index);

// Whether highlighted items get the beam.
[[nodiscard]] bool MarkerOn();
} // namespace GameLogic::Items::LootFilterRuntime
