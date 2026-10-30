// The gamepad's on-screen keyboard: while a pad is in use and a text field has the keys (the chat,
// the login's account and password, a search field), it comes up at the screen's foot (at its top
// when the field is down there) with the field's text in a line of its own, and the pad types on it.
// A finger, the mouse or a key hands the field back to the phone's keyboard or the real one.
// Its keys and pages are UI/HUD/PadKeyboardLayout's; a field that takes digits only gets the number
// pad, a password field no Russian letters and dots in place of its text.
//
// | Pad | On the keyboard |
// |---|---|
// | D-pad, left stick | Move between the keys |
// | A | Press the key |
// | B | Close it (the chat closes, as with Esc) |
// | X | Backspace (held, it repeats) |
// | Y | Space |
// | LT | Shift: once for one capital, twice to stay |
// | RT | The symbols, and back to the letters |
// | View | English or Russian letters |
// | LB, RB | Move the cursor (held, it repeats) |
// | Menu | Enter: send the chat, log in |
#pragma once

#include "Core/Input/GamepadStepper.h"

namespace UI::HUD::PadKeyboard
{
// Once a frame from UI::HUD::GamepadControls, with the pad's presses since the last frame.
// @return whether the keyboard is up and took the pad: the game gets nothing from it this frame.
bool Update(bool padInUse, const Core::Input::Gamepad::Presses& presses);
} // namespace UI::HUD::PadKeyboard
