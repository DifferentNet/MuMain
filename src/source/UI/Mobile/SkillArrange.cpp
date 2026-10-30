#include "UI/Mobile/SkillArrange.h"

#include <algorithm>
#include <cmath>

namespace UI::Mobile::SkillArrange
{
int HotKeyOf(const HotKeys& keys, int skill)
{
    if (skill == NoSkill)
        return -1;
    const auto found = std::find(keys.begin(), keys.end(), skill);
    return found == keys.end() ? -1 : static_cast<int>(found - keys.begin());
}

HotKeys Placed(HotKeys keys, int target, int skill)
{
    if (target < 0 || target >= HotKeyCount)
        return keys;
    const int from = HotKeyOf(keys, skill);
    if (from == target)
        return keys;
    const int previous = keys[target];
    keys[target] = skill;
    if (from >= 0)
        keys[from] = previous;
    return keys;
}

int NextEmptySlot(const HotKeys& keys, bool upperSet, int from)
{
    for (int step = 1; step < SlotCount; ++step)
    {
        const int slot = (from + step) % SlotCount;
        if (keys[HotKeyOfSlot(slot, upperSet)] == NoSkill)
            return slot;
    }
    return from;
}

int PageCapacity(float boxWidth, float boxHeight, float entryWidth, float entryHeight, float gap)
{
    if (entryWidth <= 0.f || entryHeight <= 0.f)
        return 1;
    const int columns = static_cast<int>(std::floor((boxWidth + gap) / (entryWidth + gap)));
    const int rows = static_cast<int>(std::floor((boxHeight + gap) / (entryHeight + gap)));
    return std::max(1, columns) * std::max(1, rows);
}

int PageCount(int entries, int perPage)
{
    if (perPage <= 0 || entries <= 0)
        return 1;
    return (entries + perPage - 1) / perPage;
}
} // namespace UI::Mobile::SkillArrange
