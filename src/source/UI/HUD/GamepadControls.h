// A gamepad laid out as an Xbox pad plays the game (the device is Core::Input::Gamepad), as Diablo's
// pads do: a row of six action buttons, the potions on the bumper, trigger and d-pad, no pointer in
// the world. It presses the on-screen controls' buttons (UI/HUD/TouchControls, shown with the pad's
// glyphs while it is in use); on the login and character screens the frame hops over their controls
// and the face buttons choose and start the character (docs/touch-controls.md).
//
// | Pad | In the world (no window open) |
// |---|---|
// | Left stick | Walks. Pushed through a gate or a map change, it waits until let go or centred |
// | Right stick | A flick picks the next target: right or down the next further, left or up the next nearer |
// | RS (the right stick pressed) | Back to the nearest target |
// | A | Attack: a tap fights the target once, held it goes on |
// | X, Y, B, RB, RT | The five skill buttons: a tap uses the skill once, held it goes on |
// | LB, LT | The potions Q and W |
// | D-pad left, right | The potions E and R |
// | D-pad up | MU Helper on and off |
// | D-pad down | Pick up: held, it gathers the drops around; with none, a press talks to the nearest NPC. The drop it would take shows its name and a prompt (UI/HUD/PadOverlay) |
// | LS (the left stick pressed) | Puts skills on the skill buttons |
// | View | The map |
// | Menu | A tap: the hero hub (UI/HUD/PadHub: the inventory, character, skills, quests, guild and settings as tabs, LB and RB turn them); a second tap, or one over another window, the menu of windows (the stick or d-pad move the highlight, A opens, B or Menu closes). Held: the chat |
//
// The world has no pointer: none is drawn. Over a game window (UI/HUD/PadFrame) there is no mouse
// either: a frame rests on a cell, a slot or a button, and the d-pad and the left stick hop it (the
// character stands), scrolling a list past its last row on view. The right stick still moves the
// pointer freely and snaps onto the nearest target when let go. A clicks at the frame (or presses the
// control there, or attacks off the windows), X puts an item on or uses it, Y shows its card first,
// RS right-clicks, B goes back (closes the window on top). The other buttons act as in the world. A
// dialog alone keeps the pointer, and d-pad up and down over a list that scrolls turn the wheel. A dialog waiting for an answer (a trade or party request, a
// question) takes A and Menu as Enter, B as Esc, after its first second; A on one of its buttons
// clicks that one. The help window (F1) turns its pages with LB and RB.
//
// A stick's own button counts only with the stick near its centre: one pushed hard clicks by accident.
//
// Off the world: the sticks move the pointer, A or RT click, B is Esc and Menu is Enter.
//
// While a text field has the keys (the chat, the login), the on-screen keyboard (UI/HUD/PadKeyboard)
// has the pad and nothing else acts.
#pragma once

namespace UI::HUD::GamepadControls
{
// Once a frame after the scene moved, before the touch controls act on what it pressed.
void Update();

// Whether the pad plays the pointer now (a window open, or off the world): only then does a monster
// under it count as aimed at.
[[nodiscard]] bool PlaysPointer();

// Whether the game's pointer is not drawn: the pad plays the world, its frame rests on a target, or
// its keyboard is up.
[[nodiscard]] bool HidesPointer();

// The drop d-pad down would pick up (an index into Items), its name shown; -1 for none.
[[nodiscard]] int PromptDrop();
// The NPC or player the right stick chose (an index into CharactersClient), its name shown as under
// the mouse; -1 for none.
[[nodiscard]] int PromptMeet();
} // namespace UI::HUD::GamepadControls
