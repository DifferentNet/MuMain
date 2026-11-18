// CharacterPreselect.cpp - which character the select screen marks on entry.

#include "Scenes/CharacterPreselect.h"

namespace Scenes
{
int PickPreselectSlot(const std::vector<std::wstring>& namesBySlot, std::wstring_view remembered)
{
    int onlySlot = -1;
    int filled = 0;

    for (int slot = 0; slot < static_cast<int>(namesBySlot.size()); ++slot)
    {
        const std::wstring& name = namesBySlot[slot];
        if (name.empty())
        {
            continue;
        }

        if (!remembered.empty() && name == remembered)
        {
            return slot;
        }

        onlySlot = slot;
        ++filled;
    }

    return filled == 1 ? onlySlot : -1;
}
} // namespace Scenes
