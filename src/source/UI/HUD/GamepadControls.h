// A gamepad laid out as an Xbox pad plays the game (the device is Core::Input::Gamepad). In the world
// it presses the on-screen controls' buttons (UI/HUD/TouchControls, shown with the pad's glyphs while
// it is in use) and plays the mouse with the right stick; on the login and character screens both
// sticks play the mouse.
//
// | Pad | In the world |
// |---|---|
// | Left stick | Walks. Pushed through a gate or a map change, it waits until let go or centred |
// | Right stick | The pointer, slower over a window |
// | A | Attack: a tap fights the nearest monster once, held it goes on. On a window or a control it clicks there |
// | B | Back: closes the window on top (with none open, the system menu) |
// | X, Y, RB, LT, D-pad down | The five skill buttons: a tap uses the skill once, held it goes on |
// | LB | Pick up: a tap takes the nearest item, held it goes on |
// | D-pad up | MU Helper on and off |
// | D-pad left, right | The potions Q and W |
// | RT | A left click at the pointer (talk to an NPC, walk there); held, the button stays down (drag) |
// | RS (the right stick pressed) | A right click at the pointer, held as long as the stick |
// | LS (the left stick pressed) | Puts skills on the skill buttons |
// | View | The map |
// | Menu | The menu of windows: the stick or d-pad moves the highlight, A opens, B or Menu closes; Chat = chat |
//
// A stick's own button counts only with the stick near its centre: one pushed hard clicks by accident.
// With the menu open, only the menu acts. A dialog waiting for an answer (a trade or party request, a
// question) takes A and Menu as Enter, B as Esc, after its first second; A on one of its buttons
// clicks that one. The help window (F1) turns its pages with LB and RB.
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
} // namespace UI::HUD::GamepadControls
