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

namespace UI::HUD::PadTargets
{
// Looks the open windows over again: once a frame while one is open. The span lasts until the next call.
[[nodiscard]] std::span<const PadTargetPick::Target> Collect();
// Whether the last Collect found a message box or a menu over the windows and took only its controls.
[[nodiscard]] bool OnDialog();
// Where the windows the workspace places start from the left after the last Collect, in context
// pixels; none while only dialogs and screens are open.
[[nodiscard]] std::optional<float> WindowsLeft();
} // namespace UI::HUD::PadTargets
