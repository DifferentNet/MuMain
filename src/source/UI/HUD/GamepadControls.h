// A gamepad laid out as an Xbox pad plays the game (the device is Core::Input::Gamepad). In the world
// it presses the on-screen controls' buttons (UI/HUD/TouchControls, shown with the pad's glyphs while
// it is in use) and plays the mouse with the right stick; on the login and character screens both
// sticks play the mouse.
//
// | Pad | In the world |
// |---|---|
// | Left stick | Walks |
// | Right stick | The pointer, slower over a window |
// | A | Held: attack. With the pointer on a window or a control it clicks there instead |
// | B | Back: closes the window on top (with none open, the system menu) |
// | X, Y, RB, LT, D-pad down | The five hotkey skills: a tap makes one current, held it casts |
// | LB | Pick up (held) |
// | D-pad up | MU Helper on and off |
// | D-pad left, right | The potions Q and W |
// | RT | A left click at the pointer (talk to an NPC, walk there); held, the button stays down (drag) |
// | R3 | A right click at the pointer |
// | L3 | The skill list, to put a skill on a button |
// | View | The map |
// | Menu | The menu of windows: the stick or d-pad moves the highlight, A opens, B or Menu closes; Chat = chat |
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
