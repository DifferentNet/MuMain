// The places the gamepad's frame rests on in the open windows (UI/HUD/GamepadControls): item cells,
// equipment slots and every control a click works (base.rcss marks them with `drag: block`; divs that
// bind a click, fields), in the game's windows (UI::Placement's or slot-placed), its dialogs and the
// screens over them, never the HUD's.
// While a message box or a menu over the windows is up, only its controls. Their boxes are in the
// RmlUi context's pixels, where they are drawn (transforms included).
#pragma once

#include "UI/HUD/PadTargetPick.h"

#include <optional>
#include <span>
#include <string_view>

namespace UI::HUD::PadTargets
{
// Looks the open windows over again: once a frame while one is open. The span lasts until the next call.
[[nodiscard]] std::span<const PadTargetPick::Target> Collect();
// Whether the last Collect found a message box or a menu over the windows and took only its controls.
[[nodiscard]] bool OnDialog();
// Out of the world: every document but the character screen's bar is a screen whose controls the frame
// rests on (the login, the server list, the character creation, their message boxes).
void SetScreens(bool screens);
// Where the windows the workspace places start from the left after the last Collect, in context
// pixels; none while only dialogs and screens are open.
[[nodiscard]] std::optional<float> WindowsLeft();
// Scrolls a list of the last Collect (a target's `list`) a step up (dy < 0) or down: a third of what it
// shows. Straight, not by a wheel turn: a wheel goes to the element under the pointer, which may not
// scroll it (the settings window took none).
void ScrollList(int list, int dy);
// Whether the target is in the document of this file name (npc_shop.rml).
[[nodiscard]] bool IsIn(const PadTargetPick::Target& target, std::string_view fileName);
} // namespace UI::HUD::PadTargets
