#include "GameLogic/Items/JewelUpgradeResult.h"

namespace GameLogic::Items::JewelUpgrade
{
namespace
{
constexpr int NoSlot = -1;

int g_pendingSlot = NoSlot;
std::uint32_t g_pendingSinceMs = 0;
} // namespace

Outcome Compare(Levels before, Levels after)
{
    if (after.level > before.level)
        return Outcome::LevelUp;
    if (after.level < before.level)
        return Outcome::LevelDown;
    if (after.optionLevel > before.optionLevel)
        return Outcome::OptionUp;
    if (after.optionLevel < before.optionLevel)
        return Outcome::OptionDown;
    return Outcome::None;
}

void RememberTarget(int slot, std::uint32_t nowMs)
{
    g_pendingSlot = slot;
    g_pendingSinceMs = nowMs;
}

bool TakeTarget(int slot, std::uint32_t nowMs)
{
    if (g_pendingSlot == NoSlot || slot != g_pendingSlot)
        return false;

    const bool inTime = nowMs - g_pendingSinceMs <= AnswerWindowMs;
    g_pendingSlot = NoSlot;
    return inTime;
}
} // namespace GameLogic::Items::JewelUpgrade
