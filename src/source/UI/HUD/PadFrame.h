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
// The windows closed: the frame is gone.
void Reset();

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
// Whether that target is a cell with no item in it.
[[nodiscard]] bool OnEmptyCell();
} // namespace UI::HUD::PadFrame
