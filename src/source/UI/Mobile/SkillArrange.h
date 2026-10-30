// Putting skills on the five skill buttons of the touch controls (UI/HUD/TouchControls' skill
// picker): the hotkeys the buttons stand for, a skill moved onto one, and the picker's pages.
//
// The sums only; the picker reads and writes the hotkeys through CSkillList.
#pragma once

#include <array>

namespace UI::Mobile::SkillArrange
{
// The keyboard's ten skill hotkeys, indexed by their digit (1-9, then 0). Each holds what the skill
// list's hotkeys hold: a skill's index in the character's list or a pet command, NoSkill when empty.
inline constexpr int HotKeyCount = 10;
inline constexpr int NoSkill = -1;
using HotKeys = std::array<int, HotKeyCount>;

// The skill buttons stand for five hotkeys: 1-5, or 6-9 and 0 as the upper set (the touch controls
// keep their own set, which the current skill does not switch).
inline constexpr int SlotCount = 5;

[[nodiscard]] constexpr int HotKeyOfSlot(int slot, bool upperSet)
{
    const int hotKey = (upperSet ? 6 : 1) + slot;
    return hotKey == HotKeyCount ? 0 : hotKey;
}

// The hotkey that holds a skill, or -1.
[[nodiscard]] int HotKeyOf(const HotKeys& keys, int skill);

// The hotkeys once `skill` goes on hotkey `target`. A skill on another hotkey already leaves it, and
// what `target` held takes its place there (the two swap); with the skill on no hotkey, what `target`
// held comes off. NoSkill empties `target`.
[[nodiscard]] HotKeys Placed(HotKeys keys, int target, int skill);

// The first slot after `from`, round the five, whose hotkey is empty; `from` when the others are
// all taken.
[[nodiscard]] int NextEmptySlot(const HotKeys& keys, bool upperSet, int from);

// How many entries fit a box in rows and columns with `gap` between them; at least one.
[[nodiscard]] int PageCapacity(float boxWidth, float boxHeight, float entryWidth, float entryHeight, float gap);

// How many pages `entries` entries fill at `perPage` a page; at least one.
[[nodiscard]] int PageCount(int entries, int perPage);
} // namespace UI::Mobile::SkillArrange
