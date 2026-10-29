// The on-screen controls of phones and tablets (the browser client played by touch), as MU's mobile
// games lay them out: a floating joystick on the left, a big attack button in the bottom right corner
// with the five hotkey skills around it, pickup and auto-hunt (the MU Helper) buttons, the two
// potion hotkeys, the chat, and a menu of the windows a keyboard opens with its letters.
//
// The document is touch_controls.rml. Fingers reach the controls through Core::Input::Touch (the
// controls answer its hit test from their boxes); what they do in the world is GameLogic::Touch.
// A finger anywhere else plays the mouse, so the game's own windows and the world work as with one.
#pragma once

namespace UI::HUD::TouchControls
{
// Once a frame, after the scene moved: shows the controls in the world on a touch device, follows
// their fingers and acts.
void Update();
} // namespace UI::HUD::TouchControls
