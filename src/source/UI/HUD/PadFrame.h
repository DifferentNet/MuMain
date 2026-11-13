// The gamepad's frame over the open windows: no free mouse there, the frame rests on an item cell, an
// equipment slot or a button (UI/HUD/PadTargets), and the pad's pointer, hidden, sits in its middle so
// A, X and Y click there. The d-pad and the left stick hop it to the next one that way (into the next
// window past a window's edge), and over a list's last row on view they scroll the list instead. The
// right stick still moves the pointer freely, and snaps it onto the nearest target as it is let go.
#pragma once

#include "Core/Input/GamepadStepper.h"
#include "UI/HUD/PadTargetPick.h"

#include <chrono>

struct SDL_Window;

namespace UI::HUD::PadFrame
{
using Clock = std::chrono::steady_clock;

// Once a frame while a game window is open, after the right stick moved the pointer: takes the d-pad's
// presses it steps with.
void Update(SDL_Window* window, Core::Input::Gamepad::Presses& presses, Clock::time_point now);
// The windows opened or changed (a hub tab turned): the frame goes to the nearest target as soon as
// there is one.
void SnapSoon();
// A menu item pressed: the window it opens takes the frame on its first target; with nothing new open
// the frame stays where it was.
void SnapOnNewWindow();
// The windows closed: the frame is gone.
void Reset();
// RB over the windows (pad round 3, 2026-11-13): the frame goes to the next window to the right (round
// to the leftmost), back to where it last rested there, else to its first target. Done in the next Update.
void NextWindow();
// How many windows the frame's targets are in (as of the last Update; 0 under a dialog).
[[nodiscard]] int WindowCount();
// The window (a target's `window`) the frame rests in, -1 for none.
[[nodiscard]] int CurrentWindow();

// Whether the frame rests on a target, and its box in the RmlUi context's pixels.
[[nodiscard]] bool Box(PadTargetPick::Box& box);
// Whether that target is an item's place (a cell, an equipment slot), not a button.
[[nodiscard]] bool OnItemPlace();
// Whether that target is an equipment slot: the pad's X takes off what is worn there.
[[nodiscard]] bool OnWornPlace();
// Whether that target is a cell of the NPC shop: A buys what is there.
[[nodiscard]] bool OnGoods();
// Whether that target is a cell of the crafting box: its items have no card.
[[nodiscard]] bool OnMixItem();
// Whether the frame rests on a slider (the pad's pointer in its middle, not moved by the right stick):
// the d-pad's sides change it, A does nothing there.
[[nodiscard]] bool OnGauge();
// Whether that target is a cell with no item in it.
[[nodiscard]] bool OnEmptyCell();
} // namespace UI::HUD::PadFrame
