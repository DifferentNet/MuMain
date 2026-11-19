// The on-screen controls of phones and tablets (the browser client played by touch), as MU's mobile
// games lay them out: a floating joystick on the left, a big attack button in the bottom right corner
// with the five hotkey skills around it, pickup and auto-hunt (the MU Helper) buttons, the two
// potion hotkeys, the chat, and a menu of the windows a keyboard opens with its letters. The skill
// picker sets which skill each skill button casts (the Skills button, Menu -> Skills, or a tap on an
// empty skill button opens it).
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

// How often the controls were measured since the last call (the MU_RENDER_TIMING log): every frame for
// fingers, about four times a second for a pad.
[[nodiscard]] std::uint32_t TakeMeasureCount();


// The controls a gamepad button stands for.
enum class PadButton : std::uint8_t
{
    Attack,
    Skill0,
    Skill1,
    Skill2,
    Skill3,
    Skill4,
    // The pad's sixth skill (RT + B): a pad alone on the lower hotkey set only.
    Skill5,
    // The pad's third page (RB held + X, Y, B, pad round 6): hotkeys 7-9, a pad alone on the lower
    // hotkey set only.
    Skill6,
    Skill7,
    Skill8,
    PickUp,
    Auto,
    Menu,
    PotionQ,
    PotionW,
    // The item hotkeys E and R: no layout shows them now (the keyboard drinks them; the pad's d-pad
    // sides open the map and the inventory since 2026-11-03).
    PotionE,
    PotionR,
    // The Social panel (the pad's View button): the players around and what to do with them.
    Social,
    Back,
    Count,
};

// Holds or lets go of a control as a finger on it would. A press counts once per hold.
void SetPadHeld(PadButton button, bool held);
// The pad's second page of skills shows (RT held, or tapped with the RT toggle): the skill buttons 4
// and 5 show in the places of the first ones while the pad plays without fingers. `potions`: RT is
// held, so the left bumper and trigger drink E and R (they never stay on the toggled page).
void SetPadAlt(bool skills, bool potions);
// The pad's third page of skills shows (RB held): the skill buttons 7 to 9 show in the places of the
// first ones. It wins over the second page while RB is held.
void SetPadThird(bool skills);
// How many skill buttons are in play: nine for a pad alone on the lower hotkey set (the sixth on RT + B,
// the seventh to ninth on RB + X, Y, B), else five. The help and the controls card say "4 to 6" or
// "4 and 5" by it, and show the RB page with nine.
[[nodiscard]] int SkillButtonsInPlay();

// The control under a window pixel (the pad's pointer), or -1. The joystick's area does not count:
// it walks only under a finger.
[[nodiscard]] int ControlAt(float x, float y);
// Holds or lets go of a control ControlAt found, as a finger on it would.
void SetControlHeld(int control, bool held);
// Lets go of everything the pad holds (the pad is put down, or leaves the world).
void ReleasePad();

// The menu, the skill picker (where the skill buttons are set) or the Social panel, for a pad: whether one is open, a
// highlighted item the stick moves (dx, dy: -1, 0 or 1 a step; 0, 0 highlights the first item when
// none is), and pressing the highlighted one. The picker's highlight goes over its skills and buttons
// and the skill buttons it sets.
[[nodiscard]] bool IsMenuOpen();
void CloseMenu();
void MoveMenuFocus(int dx, int dy);
void ActivateMenuFocus();

// Opens the Social panel (the players around and what to do with them), as its button does: the party
// window's phone button (UI/Party/PartyInfoWindow.h) asks for it.
void ShowSocialPanel();

// A skill the hero learned just now (its index in the character's skill list, what a hotkey holds, and its type),
// while the on-screen controls show: the next Update() puts it on the first empty skill button of the set in use and
// says "New skill: X (skill button N)", or only "New skill: X" with every button taken. A filled button is never
// replaced. False with the controls not shown: the caller says "New skill" itself, and the mouse's keys stay as set.
[[nodiscard]] bool OfferNewSkill(int skillIndex, int skillType);

// Opens the skill picker on the first empty skill button (the pad's L3). False with the controls not
// shown, where there is no picker.
bool OpenSkillPicker();

// The Back button's press from elsewhere (the phone's back gesture, UI::Mobile::PhonePage): the open
// menu or skill picker shuts, else the window on top closes or, with none open, the system menu
// opens. Off the world it is Esc.
void PressBack();

// A tap on a window drawn over another changed what lies under the pointer the tap left (the help's
// page turned over the inventory): that pointer leaves the window once the finger is up, so the window
// beneath shows no tooltip for an item nobody tapped. A pointer the mouse or the pad moved stays.
void RestPointerOnLift();
} // namespace UI::HUD::TouchControls
