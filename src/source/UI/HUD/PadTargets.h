// The places the gamepad's frame rests on in the open windows (UI/HUD/GamepadControls): item cells,
// equipment slots and every control a click works (base.rcss marks them with `drag: block`; divs that
// bind a click, fields), in the game's windows (UI::Placement's or slot-placed), its dialogs and the
// screens over them, never the HUD's.
// While a message box or a menu over the windows is up, only its controls. Their boxes are in the
// RmlUi context's pixels, where they are drawn (transforms included).
#pragma once

#include "UI/HUD/PadTargetPick.h"

#include <cstdint>
#include <optional>

#include <span>
#include <string_view>

namespace UI::HUD::PadTargets
{
// The targets in the open windows: asked once a frame while one is open. The windows are walked again
// only while the pad is at work (Refresh), as a document opens or closes, the screen's size changes or
// a target's element goes, and every 100 ms otherwise; else the last walk's targets come back. The span
// lasts until the next call.
[[nodiscard]] std::span<const PadTargetPick::Target> Collect();
// The next Collects, for a while, walk the windows every frame: the pad was pressed or pushed, a list
// scrolled, a page turned, the frame waits for a window to show.
void Refresh();
// How often the windows were walked since the last call (the MU_RENDER_TIMING log).
[[nodiscard]] std::uint32_t TakeCollectCount();
// Whether the last Collect found a message box or a menu over the windows and took only its controls.
[[nodiscard]] bool OnDialog();
// Whether the last Collect found a dropdown's list open (the settings window's) and took only its options.
[[nodiscard]] bool OnPopup();
// Out of the world: every document but the character screen's bar is a screen whose controls the frame
// rests on (the login, the server list, the character creation, their message boxes).
void SetScreens(bool screens);
// Where the windows the workspace places start from the left after the last Collect, in context
// pixels; none while only dialogs and screens are open.
[[nodiscard]] std::optional<float> WindowsLeft();
// Scrolls a list of the last Collect (a target's `list`) a step up (dy < 0) or down: a third of what it
// shows. Straight, not by a wheel turn: a wheel goes to the element under the pointer, which may not
// scroll it (the settings window took none). False when it did not move (already at that end).
bool ScrollList(int list, int dy);
// Steps a slider of the last Collect (a target's `gauge`) one step left (dx < 0) or right, as the arrow
// keys do on it.
void StepGauge(int target, int dx);
// A window's pages (pad round 3, 2026-11-13): the document at `window` (a target's `window`, or -1 for
// the first open window that has pages) turns to its next page. Its markup marks them: a "pad-page-next"
// control (the vault's arrow) is clicked, else of its "pad-page" tabs the one after the "active" or
// "selected" one, round to the first (a "locked" or "hidden" tab is passed by); a tab that acts
// on the press alone (the guild window's) is pressed. The window turned, -1 with no pages.
int TurnPage(int window);
// Whether the page TurnPage last turned to shows (pad round 8): its tab is now the active one (the window's
// view has taken the turn: the old page's targets are gone), or it was a next-page arrow, whose page shows
// at once in the same places.
[[nodiscard]] bool TurnShown();
// The target of the last Collect that TurnPage last pressed (the tab turned to), None when it is none.
[[nodiscard]] int TurnedControl();
// Whether TurnPage would find pages there.
[[nodiscard]] bool HasPages(int window);
// Whether the target is in the document of this file name (npc_shop.rml).
[[nodiscard]] bool IsIn(const PadTargetPick::Target& target, std::string_view fileName);
} // namespace UI::HUD::PadTargets
