#pragma once

#include <cstdint>

// What a Jewel of Bless, Soul or Life did to the item it was used on. The server answers every used jewel with the
// changed item, on success and on failure alike (Soul can drop the level, Life can fail), and the client only swaps
// the item; this tells the player the result. Only the item the player just used a jewel on counts: the same answer
// also carries other item changes.
namespace GameLogic::Items::JewelUpgrade
{
// The item's level (+0..+15) and its additional option level, before and after the jewel.
struct Levels
{
    int level = 0;
    int optionLevel = 0;
};

enum class Outcome
{
    None,      // nothing comparable changed: no line
    LevelUp,   // Bless or Soul worked
    LevelDown, // Soul failed: the item lost one level or went back to +0
    OptionUp,  // Life added or raised the option at the same level
    OptionDown, // Life failed: the option was lowered or removed at the same level
};

[[nodiscard]] Outcome Compare(Levels before, Levels after);

// How long after a jewel is used its answer still counts (the server answers at once; a lost answer must not make a
// later, unrelated change of the same slot speak).
constexpr std::uint32_t AnswerWindowMs = 10'000;

// The player used a jewel on the item in this bag slot.
void RememberTarget(int slot, std::uint32_t nowMs);

// Whether the item change for this slot answers the jewel just used on it; a match is used up.
[[nodiscard]] bool TakeTarget(int slot, std::uint32_t nowMs);
} // namespace GameLogic::Items::JewelUpgrade
