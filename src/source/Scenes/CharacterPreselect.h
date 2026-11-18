#pragma once

// CharacterPreselect.h - which character the select screen marks on entry.

#include <string>
#include <string_view>
#include <vector>

namespace Scenes
{
// Slot to preselect: the slot whose name equals `remembered`, else the only
// filled slot, else -1. An empty name means an empty slot.
[[nodiscard]] int PickPreselectSlot(const std::vector<std::wstring>& namesBySlot, std::wstring_view remembered);
} // namespace Scenes
