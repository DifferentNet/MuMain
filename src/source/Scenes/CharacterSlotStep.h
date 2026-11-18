#pragma once

// Character select by keyboard (Left / Right): the slot the selection moves to.
namespace Scenes::CharacterSelect
{
// The next slot from `current` in the direction of `step` (+1 or -1) that `filled` accepts, wrapping
// around the `slotCount` slots. From no selection (-1) Right picks the first filled slot and Left the
// last. `current` when no other slot is filled.
template <typename Filled>
int StepToFilledSlot(int current, int step, int slotCount, Filled filled)
{
    if (slotCount <= 0 || step == 0)
    {
        return current;
    }
    int slot = current;
    if (current < 0 || current >= slotCount)
    {
        slot = step > 0 ? -1 : slotCount;
    }
    for (int tried = 0; tried < slotCount; ++tried)
    {
        slot = ((slot + step) % slotCount + slotCount) % slotCount;
        if (filled(slot))
        {
            return slot;
        }
    }
    return current;
}
} // namespace Scenes::CharacterSelect
