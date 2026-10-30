// The on-screen controls of phones and tablets (the browser client played by touch), as MU's mobile
// games lay them out: a floating joystick on the left, a big attack button in the bottom right corner
// with the five hotkey skills around it, pickup and auto-hunt (the MU Helper) buttons, the two
// potion hotkeys, the chat, and a menu of the windows a keyboard opens with its letters.
//
// The document is touch_controls.rml. Fingers reach the controls through Core::Input::Touch (the
// controls answer its hit test from their boxes); what they do in the world is GameLogic::Touch.
// A finger anywhere else plays the mouse, so the game's own windows and the world work as with one.
//
// A gamepad presses the same controls (UI/HUD/GamepadControls): they show while one is in use too,
// each with its pad button's glyph, and the pad picks the menu's items with a highlight.
#pragma once

#include <cstdint>

namespace UI::HUD::TouchControls
{
// Once a frame, after the scene moved: shows the controls in the world on a touch device or with a
// gamepad in use, follows their fingers and the pad's buttons, and acts.
void Update();

// The controls a gamepad button stands for.
enum class PadButton : std::uint8_t
{
    Attack,
    Skill0,
    Skill1,
    Skill2,
    Skill3,
    Skill4,
    PickUp,
    Auto,
    Menu,
    PotionQ,
    PotionW,
    Map,
    Back,
    Count,
};

// Holds or lets go of a control as a finger on it would. A press counts once per hold.
void SetPadHeld(PadButton button, bool held);

// The control under a window pixel (the pad's pointer), or -1. The joystick's area does not count:
// it walks only under a finger.
[[nodiscard]] int ControlAt(float x, float y);
// Holds or lets go of a control ControlAt found, as a finger on it would.
void SetControlHeld(int control, bool held);
// Lets go of everything the pad holds (the pad is put down, or leaves the world).
void ReleasePad();

// The menu for a pad: whether it is open, a highlighted item the stick moves (dx, dy: -1, 0 or 1 a
// step; 0, 0 highlights the first item when none is), and opening the highlighted one.
[[nodiscard]] bool IsMenuOpen();
void CloseMenu();
void MoveMenuFocus(int dx, int dy);
void ActivateMenuFocus();

// The Back button's press from elsewhere (the phone's back gesture, UI::Mobile::PhonePage): the open
// menu shuts, else the window on top closes or, with none open, the system menu opens. Off the world
// it is Esc.
void PressBack();
} // namespace UI::HUD::TouchControls
