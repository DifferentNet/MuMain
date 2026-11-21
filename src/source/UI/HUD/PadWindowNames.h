// The documents the gamepad's frame (UI/HUD/PadTargets) takes for windows by their file name
// ("friend_shell.rml"), apart from the ones the workspace places or its depth tells: pure, for the tests
// (pad round 8, A1).
#pragma once

#include <string_view>

namespace UI::HUD::PadWindowNames
{
// The documents over the windows that are no windows themselves: the HUD's bar, the pad's own frame, the
// tooltips.
[[nodiscard]] bool IsNotWindow(std::string_view name);
// The windows that place themselves, drawn under the workspace's windows with no slot of their own: the
// MU Helper's skill picker, the friend list, its letters and chat rooms. In the world they are windows the
// frame rests on.
[[nodiscard]] bool IsFloatingWindow(std::string_view name);
// A floating window that hangs off the window under it (the skill picker beside the MU Helper's settings):
// while it is open the frame rests only on it.
[[nodiscard]] bool IsPopupWindow(std::string_view name);
// The dialog over the world that is no game window: the reconnect's. While it shows, in the world too, it
// is a window (a dialog by its depth), so the frame rests on its Cancel.
[[nodiscard]] bool IsWorldDialog(std::string_view name);
} // namespace UI::HUD::PadWindowNames
