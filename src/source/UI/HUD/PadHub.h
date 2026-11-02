// The gamepad's hero hub: Menu with no window open opens the character's windows one at a time, as
// tabs (the inventory, the character, the skill picker, the quests, the guild, the settings), and LB
// and RB turn from one to the next. Its tab strip is drawn over the windows by UI/HUD/PadOverlay.
// The hub ends when its window closes (B, Esc, its own X), or when Menu opens the full menu instead.
#pragma once

#include <chrono>

namespace UI::HUD::PadHub
{
using Clock = std::chrono::steady_clock;

[[nodiscard]] bool IsOpen();
// Opens the hub on the tab it was last on (the inventory the first time).
void Open(Clock::time_point now);
// Ends the hub and closes its window.
void Close();
// To the next tab (step 1, RB) or the one before (-1, LB), round the ends.
void Turn(int step, Clock::time_point now);
// Once a frame: the hub ends when its window was closed by other means.
void Update(Clock::time_point now);

// The tab strip.
[[nodiscard]] int TabCount();
[[nodiscard]] int CurrentTab();
[[nodiscard]] const wchar_t* TabLabel(int tab);
// The current tab is the skill picker (UI/HUD/TouchControls' menu): the pad plays it as the menu.
[[nodiscard]] bool IsOnSkills();
} // namespace UI::HUD::PadHub
