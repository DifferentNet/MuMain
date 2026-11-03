#include "stdafx.h"
#include "UI/HUD/GamepadControls.h"

#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadStepper.h"
#include "Core/Input/GamepadStick.h"
#include "Core/Input/GamepadStickClick.h"
#include "Core/Input/TouchInput.h"
#include "Core/Input/UiInputRouter.h"
#include "Core/Platform/GameWindow.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Touch/TouchActions.h"
#include "I18N/All.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Character/CharMakeWin.h"
#include "Character/CharSelMainWin.h"
#include "Scenes/CharacterScene.h"
#include "Scenes/SceneCommon.h"
#include "Scenes/SceneCore.h"
#include "UI/Chat/MessageType.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/HelpWindow.h"
#include "UI/Dialogs/ModalDialogs.h"
#include "UI/HUD/MainFrameWindow.h"
#include "UI/HUD/PadFrame.h"
#include "UI/HUD/PadHub.h"
#include "UI/HUD/PadKeyboard.h"
#include "UI/HUD/PadOverlay.h"
#include "UI/HUD/PadTargets.h"
#include "UI/HUD/QuickCommandWindow.h"
#include "UI/HUD/SkillList.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Inventory/InventoryContents.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/Windows/MsgWin.h"
#include "UI/Windows/SysMenuWin.h"

#include <RmlUi/Core/Context.h>
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebKeyboard.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace
{
namespace Pad = Core::Input::Gamepad;
namespace Pointer = Core::Input::Gamepad::Pointer;
namespace Controls = UI::HUD::TouchControls;
using Controls::PadButton;
using Pad::Button;
using Clock = std::chrono::steady_clock;

// The menu's highlight moves at once with the stick or the d-pad, again after this while it is
// held, then at the second pace.
constexpr auto MenuFirstRepeat = std::chrono::milliseconds(400);
constexpr auto MenuRepeat = std::chrono::milliseconds(150);
// A frame longer than this (a hitch, a hidden tab) moves the pointer no further.
constexpr float LongestFrame = 0.1f;
// GameLogic::Touch::Steer reads a direction in screen pixels: the stick's -1 .. 1, scaled up.
constexpr float StickToScreen = 100.f;
// A dialog takes the pad's answer (A, Menu) once it has been up this long, and A once it has rested
// this long too: a press meant for the fight as a trade or party request came up does not accept it.
constexpr auto DialogAnswerDelay = std::chrono::milliseconds(1000);
// The d-pad over a list that scrolls turns the wheel at once, again after this while held, then at
// the second pace.
constexpr auto WheelFirstRepeat = std::chrono::milliseconds(400);
constexpr auto WheelRepeat = std::chrono::milliseconds(110);

// Menu held this long opens the chat line instead of the menu.
constexpr auto ChatHold = std::chrono::milliseconds(500);
// D-pad up or left held this long: the MU Helper's settings, the Teleport window (ghinzuro, 2026-11-04).
constexpr auto DpadHold = std::chrono::milliseconds(500);
// D-pad down talks to an NPC this near when no drop lies around.
constexpr int NpcReach = 6;
// A chosen player's menu opens this near (the world's units; it closes past 300), after a walk up to
// them of at most this long.
constexpr float PlayerMenuDistance = 250.f;
constexpr auto PlayerMenuWalkTime = std::chrono::seconds(6);
// The right stick picks the next target once it leans this far, and again once back under the second.
constexpr float FlickLean = 0.6f;
constexpr float FlickRest = 0.3f;

// When a pad button presses its control: always, only in the world (no window open: the right stick
// picks targets), only while a window is open (the right stick plays the pointer), or anywhere but over
// a game window (the d-pad hops the frame there, UI/HUD/PadFrame).
enum class When : std::uint8_t
{
    Always,
    World,
    Window,
    NoFrame,
};

// Which page of skills a mapping belongs to: the right trigger held turns X and Y to the skill
// buttons 4 and 5 (ghinzuro, 2026-11-03: three skills on the screen, RT held for the rest).
enum class Page : std::uint8_t
{
    Any,
    Main,
    Alt,
};

// The pad buttons that press a control as long as they are held, laid out as Diablo's pads: A attacks,
// X, Y and B cast (with RT held, X and Y the next skills), the bumper and trigger on the left drink.
struct Mapping
{
    Button pad;
    PadButton control;
    When when;
    Page page = Page::Any;
};
constexpr Mapping Mappings[] = {
    {Button::A, PadButton::Attack, When::World},
    {Button::X, PadButton::Skill0, When::World, Page::Main},
    {Button::Y, PadButton::Skill1, When::World, Page::Main},
    {Button::B, PadButton::Skill2, When::World, Page::Main},
    {Button::X, PadButton::Skill3, When::World, Page::Alt},
    {Button::Y, PadButton::Skill4, When::World, Page::Alt},
    {Button::B, PadButton::Back, When::Window},
    {Button::LeftShoulder, PadButton::PotionQ, When::Always},
    {Button::LeftTrigger, PadButton::PotionW, When::Always},
    {Button::View, PadButton::Social, When::Always},
};

bool Applies(const Mapping& mapping, bool windowOpen, bool frameOpen, bool alt)
{
    if (mapping.page == Page::Main && alt)
        return false;
    if (mapping.page == Page::Alt && !alt)
        return false;
    switch (mapping.when)
    {
    case When::Always:
        return true;
    case When::NoFrame:
        return !frameOpen;
    case When::Window:
        return windowOpen;
    default:
        return !windowOpen;
    }
}

// What A does while held with a window open, decided as it is pressed.
enum class Hold : std::uint8_t
{
    None,
    // The attack button (the pointer off the windows).
    Attack,
    // The left mouse button at the pointer.
    Click,
    // An on-screen control the pointer is on.
    Control,
};

struct PointerPress
{
    Hold mode = Hold::None;
    int control = -1;
};

// D-pad up or down pressed over a list that scrolls: the wheel there, as long as it is held.
struct DpadWheel
{
    Button button;
    int steps;
    bool turning = false;
    Pad::RepeatButton repeat{WheelFirstRepeat, WheelRepeat};
};

// A button that does one thing on a tap and another held: the tap acts as it is let go.
struct LongPress
{
    bool down = false;
    // The hold went off: the release does nothing more.
    bool fired = false;
    Clock::time_point since{};
};

enum class PressKind : std::uint8_t
{
    None,
    Tap,
    Hold,
};

struct State
{
    bool inUse = false;
    // The pad had the pointer before: it takes it again where the mouse left it.
    bool pointerTaken = false;
    bool inWorld = false;
    // The hint on how to play with a pad, once a session.
    bool told = false;
    std::array<std::uint32_t, Pad::ButtonCount> seen{};
    // Buttons held while the menu was open: they act in the world only once let go, so the B that
    // closed the menu does not go on to press Back.
    std::array<bool, Pad::ButtonCount> waitForRelease{};
    PointerPress a;
    // A window is open: the right stick plays the pointer, A clicks, X right-clicks, B goes back.
    bool windowOpen = false;
    // A game window (not only a dialog) is open: the d-pad and the left stick hop the frame.
    bool frameOpen = false;
    // The character screen with nothing over it: the face buttons choose and start the character.
    bool onCharacters = false;
    // The drop d-pad down would pick up, prompted under it; -1 for none.
    int promptDrop = -1;
    // Menu held (a hold opens the chat line, then its release opens no menu), d-pad up and left.
    LongPress menuPress;
    LongPress dpadUpPress;
    LongPress dpadLeftPress;
    // D-pad down's press talks to an NPC (no drop around), rather than holding the pickup button.
    bool dpadDownTalks = false;
    // The NPC click: the pointer moved onto the NPC (1), the button down (2).
    int npcClickStage = 0;
    // The player A chose (an index in CharactersClient, -1 for none), walked up to until their menu
    // opens, and until when.
    int menuWalk = -1;
    Clock::time_point menuWalkUntil{};
    // The right stick leaned to pick a target, not yet back.
    bool flicked = false;
    // When A was last pressed or held: a dialog takes A as its answer only once it has rested.
    Clock::time_point aActive{};
    std::array<DpadWheel, 2> wheels{{{Button::DpadUp, 1}, {Button::DpadDown, -1}}};
    // The sticks' own buttons, pressed on purpose (Core/Input/GamepadStickClick.h).
    Pad::StickClick leftStickClick;
    Pad::StickClick rightStickClick;
    // The left stick walks, or waits to be let go after a map change (GameLogic::Touch::PauseSteering).
    bool steering = false;
    // Since when a dialog waits for an answer (UI/Dialogs/ModalDialogs.h).
    bool dialogOpen = false;
    Clock::time_point dialogSince{};
    // The on-screen keyboard has the pad (UI/HUD/PadKeyboard).
    bool typing = false;
    Clock::time_point lastFrame{};
    Pad::Stepper menuStepper{MenuFirstRepeat, MenuRepeat};
};
State g_state;

using Pad::Presses;

bool Held(Button button)
{
    return Pad::GetButton(button).held;
}

bool Pressed(const Presses& presses, Button button)
{
    return presses[static_cast<int>(button)];
}

// Reads a press and takes it, so what reads the presses after it does not act on it too.
bool Take(Presses& presses, Button button)
{
    const bool pressed = Pressed(presses, button);
    presses[static_cast<int>(button)] = false;
    return pressed;
}

// The buttons pressed since the last frame; reading forgets them.
Presses TakePresses()
{
    Presses presses{};
    for (int button = 0; button < Pad::ButtonCount; ++button)
    {
        const std::uint32_t count = Pad::GetButton(static_cast<Button>(button)).presses;
        presses[button] = count != g_state.seen[button];
        g_state.seen[button] = count;
    }
    return presses;
}

float FrameSeconds(Clock::time_point now)
{
    const bool first = g_state.lastFrame == Clock::time_point{};
    const float seconds = first ? 0.f : std::chrono::duration<float>(now - g_state.lastFrame).count();
    g_state.lastFrame = now;
    return std::clamp(seconds, 0.f, LongestFrame);
}

bool IsInWorld()
{
    return SceneFlag == MAIN_SCENE && LoadingWorld < 30 && Hero != nullptr;
}

// The pointer is on a window, the HUD's bar or a dialog: the four tests the world's own click uses
// before it picks a target (Input/Selection.cpp).
bool IsPointerOverUi()
{
    return MouseOnWindow || (g_pMainFrame != nullptr && g_pMainFrame->IsMouseOverHud()) ||
           g_pNewUISystem->CheckMouseUse() || Core::Input::IsMouseOverUI();
}

void PointerPosition(float& x, float& y)
{
    SDL_GetMouseState(&x, &y);
}

void StartPress(SDL_Window* window, PointerPress& press, bool attacksOffUi)
{
    float x = 0.f;
    float y = 0.f;
    PointerPosition(x, y);
    const int control = Controls::ControlAt(x, y);
    if (control >= 0)
    {
        press = {Hold::Control, control};
        Controls::SetControlHeld(control, true);
    }
    else if (attacksOffUi && !IsPointerOverUi())
    {
        press = {Hold::Attack, -1};
        Controls::SetPadHeld(PadButton::Attack, true);
    }
    else
    {
        press = {Hold::Click, -1};
        Pointer::SetButton(window, SDL_BUTTON_LEFT, true);
    }
}

// Lets go of what a press holds, unless the other press holds the same.
void EndPress(SDL_Window* window, PointerPress& press, const PointerPress& other)
{
    switch (press.mode)
    {
    case Hold::Attack:
        Controls::SetPadHeld(PadButton::Attack, false);
        break;
    case Hold::Control:
        if (other.mode != Hold::Control || other.control != press.control)
            Controls::SetControlHeld(press.control, false);
        break;
    case Hold::Click:
        if (other.mode != Hold::Click)
            Pointer::SetButton(window, SDL_BUTTON_LEFT, false);
        break;
    default:
        break;
    }
    press = {};
}

void FollowPress(SDL_Window* window, PointerPress& press, const PointerPress& other, bool pressed, bool held,
                 bool attacksOffUi)
{
    if (pressed && press.mode == Hold::None)
        StartPress(window, press, attacksOffUi);
    if (!held && press.mode != Hold::None)
        EndPress(window, press, other);
}

void StopWalking()
{
    if (!g_state.steering)
        return;
    GameLogic::Touch::StopSteering();
    g_state.steering = false;
}

// The world was left or came back (a gate, a map change, the character screen) under a pushed stick:
// the walk stops, and the push waits until the stick is let go or centred.
void PauseWalking()
{
    if (g_state.steering)
        GameLogic::Touch::PauseSteering();
}

// A push past the edge of the dead zone walks; a worn stick resting there does not (Pad::IsPushed).
void Walk(Pad::StickPosition stick)
{
    if (!Pad::IsPushed(stick, g_state.steering))
    {
        GameLogic::Touch::SetPadAim(0.f, 0.f);
        StopWalking();
        return;
    }
    // Teleport jumps this way even where no walk goes (GameLogic::Touch::PadTeleportTile).
    GameLogic::Touch::SetPadAim(stick.x, stick.y);
    GameLogic::Touch::Steer(stick.x * StickToScreen, stick.y * StickToScreen);
    g_state.steering = true;
}

void ReleaseControls()
{
    for (const Mapping& mapping : Mappings)
        Controls::SetPadHeld(mapping.control, false);
    Controls::SetPadAlt(false);
    Controls::SetPadHeld(PadButton::PickUp, false);
    Controls::SetPadHeld(PadButton::Menu, false);
}

// Whether a button holds its control: held, and let go since the menu last had it.
bool HoldsControl(Button button)
{
    bool& waiting = g_state.waitForRelease[static_cast<int>(button)];
    if (waiting && !Held(button))
        waiting = false;
    return Held(button) && !waiting;
}

// Reads a tap-or-hold button: Hold once it was held for `hold`, Tap as it is let go sooner. A tap
// shorter than a frame still counts.
PressKind ReadLongPress(LongPress& press, Button button, const Presses& presses, Clock::time_point now,
                        Clock::duration hold)
{
    const bool waiting = g_state.waitForRelease[static_cast<int>(button)];
    const bool held = HoldsControl(button);
    if (held && !press.down)
    {
        press.down = true;
        press.fired = false;
        press.since = now;
    }
    PressKind kind = PressKind::None;
    if (held && !press.fired && now - press.since >= hold)
    {
        press.fired = true;
        kind = PressKind::Hold;
    }
    const bool quick = !held && !press.down && !waiting && Pressed(presses, button);
    if ((!held && press.down && !press.fired) || quick)
        kind = PressKind::Tap;
    if (!held)
        press.down = false;
    return kind;
}

void ForgetLongPresses()
{
    g_state.menuPress = {};
    g_state.dpadUpPress = {};
    g_state.dpadLeftPress = {};
}

void WaitForRelease()
{
    for (int button = 0; button < Pad::ButtonCount; ++button)
        g_state.waitForRelease[button] = Held(static_cast<Button>(button));
}

// Lets go of the presses, the controls and the mouse buttons the pad holds.
void ReleaseButtons(SDL_Window* window)
{
    EndPress(window, g_state.a, {});
    ReleaseControls();
    g_state.npcClickStage = 0;
    ForgetLongPresses();
    Controls::ReleasePad();
    Pointer::Release(window);
    g_state.menuStepper.Reset();
}

// Lets go of everything the pad holds: it was put down, or the on-screen keyboard took it.
void ReleaseAll(SDL_Window* window)
{
    ReleaseButtons(window);
    StopWalking();
}

// The world was left or came back: the buttons let go, a pushed stick waits.
void ReleaseForWorldChange(SDL_Window* window)
{
    ReleaseButtons(window);
    PauseWalking();
    // The frame starts anew on the other side: on the screen's first control, or the first window opened.
    UI::HUD::PadFrame::Reset();
    g_state.frameOpen = false;
    g_state.windowOpen = false;
    g_state.onCharacters = false;
}

void TellOnce()
{
    if (g_state.told)
        return;
    g_state.told = true;
    g_pSystemLogBox->AddText(I18N::Game::PadConnected, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

// Presses the Menu control once: the menu of windows opens (or shuts).
void TapMenuControl()
{
    Controls::SetPadHeld(PadButton::Menu, true);
    Controls::SetPadHeld(PadButton::Menu, false);
}

// The hub on its skill picker tab: LB and RB turn its tabs, B closes it, Menu goes to the full menu.
void PlayHubOnSkills(const Presses& presses, Clock::time_point now)
{
    if (Pressed(presses, Button::LeftShoulder))
        UI::HUD::PadHub::Turn(-1, now);
    else if (Pressed(presses, Button::RightShoulder))
        UI::HUD::PadHub::Turn(1, now);
    else if (Pressed(presses, Button::B))
        UI::HUD::PadHub::Close();
    else if (Pressed(presses, Button::Menu))
    {
        UI::HUD::PadHub::Close();
        TapMenuControl();
    }
    UI::HUD::PadFrame::SnapSoon();
}

// The menu (or the skill picker, or the Social panel) open: the stick and the d-pad move its
// highlight, A opens the item, B or Menu closes it (View too, as it opened the Social panel).
// Nothing else acts: the pointer and its buttons rest too.
void PlayMenu(SDL_Window* window, const Presses& presses, Clock::time_point now)
{
    StopWalking();
    EndPress(window, g_state.a, {});
    Pointer::SetButton(window, SDL_BUTTON_RIGHT, false);
    ReleaseControls();
    WaitForRelease();
    ForgetLongPresses();

    Controls::MoveMenuFocus(0, 0);
    const Pad::Step step = g_state.menuStepper.Take(presses, now);
    if (step.dx != 0 || step.dy != 0)
        Controls::MoveMenuFocus(step.dx, step.dy);
    if (Pressed(presses, Button::A))
    {
        Controls::ActivateMenuFocus();
        // The window the item opens starts the frame on its first target, not by the last window's place.
        UI::HUD::PadFrame::SnapOnNewWindow();
    }
    if (UI::HUD::PadHub::IsOnSkills())
    {
        PlayHubOnSkills(presses, now);
        return;
    }
    if (Pressed(presses, Button::B) || Pressed(presses, Button::Menu) || Pressed(presses, Button::View))
        Controls::CloseMenu();
}

// The sticks' own buttons count only when pressed on purpose, read every frame the pad plays.
void UpdateStickClicks(const Presses& presses, Clock::time_point now)
{
    g_state.leftStickClick.Update(Pad::Lean(Pad::LeftStick()), Pressed(presses, Button::LeftStick),
                                  Held(Button::LeftStick), now);
    g_state.rightStickClick.Update(Pad::Lean(Pad::RightStick()), Pressed(presses, Button::RightStick),
                                   Held(Button::RightStick), now);
}

// With a window open, X, Y and RS are the right mouse button at the pointer (an item put on, a skill
// held on a monster), down as long as they are; a click shorter than a frame still clicks once. Y asks
// with the item card first (UI::Mobile::ClickSource::IsPadCardClick), X and RS act at once.
void PressRightButton(SDL_Window* window, const Presses& presses)
{
    const Pad::StickClick& click = g_state.rightStickClick;
    const bool xWaits = g_state.waitForRelease[static_cast<int>(Button::X)];
    const bool yWaits = g_state.waitForRelease[static_cast<int>(Button::Y)];
    const bool yHeld = HoldsControl(Button::Y);
    const bool yTapped = Pressed(presses, Button::Y) && !yWaits;
    const bool held = (click.Held() && HoldsControl(Button::RightStick)) || HoldsControl(Button::X) || yHeld;
    const bool tapped = click.Pressed() || (Pressed(presses, Button::X) && !xWaits) || yTapped;
    if ((tapped || held) && !Pointer::IsButtonHeld(SDL_BUTTON_RIGHT))
        Pointer::SetCardClick(yHeld || yTapped);
    if (tapped && !held)
        Pointer::SetButton(window, SDL_BUTTON_RIGHT, true);
    Pointer::SetButton(window, SDL_BUTTON_RIGHT, held);
}

// A window open: the right stick moves the pointer, A is its left button (a click, a drag) or the
// control under it, X and RS its right button.
void PlayPointer(SDL_Window* window, Presses& presses, float seconds)
{
    const Pad::StickPosition pointer = Pad::RightStick();
    Pointer::Move(window, pointer.x, pointer.y, seconds, IsPointerOverUi());
    FollowPress(window, g_state.a, {}, Take(presses, Button::A), Held(Button::A), true);
    PressRightButton(window, presses);
}

// No window open: a flick of the right stick picks the next target (right or down the next further,
// left or up the next nearer), RS goes back to the nearest one.
void PickTargets()
{
    const Pad::StickPosition stick = Pad::RightStick();
    const float lean = Pad::Lean(stick);
    if (g_state.flicked && lean < FlickRest)
        g_state.flicked = false;
    if (!g_state.flicked && lean >= FlickLean)
    {
        g_state.flicked = true;
        const float along = std::abs(stick.x) >= std::abs(stick.y) ? stick.x : stick.y;
        GameLogic::Touch::CycleTarget(along >= 0.f ? 1 : -1);
    }
    if (g_state.rightStickClick.Pressed())
        GameLogic::Touch::ClearChosenTarget();
}

// Whether a dialog waits for an answer and has been up long enough to take one from the pad.
bool DialogTakesAnswers(Clock::time_point now)
{
    const bool open = UI::Dialogs::Modal::IsOpen();
    if (open && !g_state.dialogOpen)
        g_state.dialogSince = now;
    g_state.dialogOpen = open;
    return open && now - g_state.dialogSince >= DialogAnswerDelay;
}

// Whether A at the pointer presses something there: an on-screen control or a button RmlUi draws (a
// dialog's own answers among them).
bool IsPointerOnButton()
{
    float x = 0.f;
    float y = 0.f;
    PointerPosition(x, y);
    return Controls::ControlAt(x, y) >= 0 || UI::Dialogs::Modal::IsPointerOnButton();
}

// A dialog's answer: Menu, and A off its buttons, press Enter (yes, OK). B is Esc already (Back).
// A answers only once it rested: A tapped over and over in a fight goes on attacking.
void AnswerDialog(Presses& presses, bool aRested)
{
    if (Take(presses, Button::Menu))
    {
        Core::Input::Touch::TapKey(VK_RETURN);
        // Still held as the dialog goes, it does not go on to open the menu.
        g_state.waitForRelease[static_cast<int>(Button::Menu)] = true;
    }
    if (aRested && Pressed(presses, Button::A) && g_state.a.mode == Hold::None && !IsPointerOnButton())
    {
        Take(presses, Button::A);
        Core::Input::Touch::TapKey(VK_RETURN);
    }
}

// The help window (F1) open: LB and RB turn its pages back and on (RB past the last one closes it),
// instead of picking up and casting.
void TurnHelpPages(Presses& presses)
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_HELP))
        return;
    for (const Button button : {Button::LeftShoulder, Button::RightShoulder})
    {
        if (Held(button))
            g_state.waitForRelease[static_cast<int>(button)] = true;
    }
    if (Take(presses, Button::LeftShoulder))
        g_pHelp->TurnPage(-1);
    if (Take(presses, Button::RightShoulder))
        g_pHelp->TurnPage(1);
}

// An item on the cursor over a window: B puts it back where it was taken from, the window stays; Y
// drops it to the ground under the hero, as a click on the ground would.
void PlayHeldItem(Presses& presses)
{
    if (!UI::Inventory::HasPickedItem())
        return;
    if (Take(presses, Button::B))
    {
        g_state.waitForRelease[static_cast<int>(Button::B)] = true;
        UI::Inventory::RestorePickedItem();
    }
    else if (Take(presses, Button::Y))
    {
        g_state.waitForRelease[static_cast<int>(Button::Y)] = true;
        g_pMyInventory->DropHeldItemAtFeet();
    }
}

// The hero hub open: LB and RB turn its tabs, instead of drinking and casting.
void TurnHubTabs(Presses& presses, Clock::time_point now)
{
    if (!UI::HUD::PadHub::IsOpen())
        return;
    for (const Button button : {Button::LeftShoulder, Button::RightShoulder})
    {
        if (Held(button))
            g_state.waitForRelease[static_cast<int>(button)] = true;
    }
    int step = 0;
    if (Take(presses, Button::LeftShoulder))
        step = -1;
    else if (Take(presses, Button::RightShoulder))
        step = 1;
    if (step == 0)
        return;
    UI::HUD::PadHub::Turn(step, now);
    UI::HUD::PadFrame::SnapSoon();
}

// Over a list that scrolls (a shop, the quests, the chat's scrollbar), d-pad up and down turn the mouse
// wheel there instead of pressing Auto and the fifth skill: at once, and on while held. A press that
// began off such a list goes on pressing its control.
void TurnWheelWithDpad(SDL_Window* window, Presses& presses, Clock::time_point now)
{
    std::optional<bool> overList;
    for (DpadWheel& wheel : g_state.wheels)
    {
        const bool pressed = Pressed(presses, wheel.button);
        if (pressed)
        {
            if (!overList)
            {
                float x = 0.f;
                float y = 0.f;
                PointerPosition(x, y);
                overList = Core::Input::UiScrollsAt(x, y);
            }
            wheel.turning = *overList;
        }
        if (!wheel.turning)
            continue;
        Take(presses, wheel.button);
        const bool held = Held(wheel.button);
        // Its control acts again only once the button is let go.
        g_state.waitForRelease[static_cast<int>(wheel.button)] = held;
        if (wheel.repeat.Take(pressed, held, now))
            Pointer::TurnWheel(window, wheel.steps);
        wheel.turning = held;
    }
}

// The buttons that press a control hold it while held. A tap shorter than a frame (a quick one on a
// slow phone, the press that woke the pad in a browser) still presses it once. A button for the other
// mode lets its control go (B is a skill in the world, Back over a window).
void HoldMappedControls(const Presses& presses)
{
    std::array<bool, static_cast<int>(PadButton::Count)> held{};
    // The second page shows only in the world: over a window X and Y act on the items.
    const bool alt = Held(Button::RightTrigger) && !g_state.windowOpen;
    Controls::SetPadAlt(alt);
    for (const Mapping& mapping : Mappings)
    {
        if (!Applies(mapping, g_state.windowOpen, g_state.frameOpen, alt))
            continue;
        const bool holds = HoldsControl(mapping.pad);
        if (!holds && Pressed(presses, mapping.pad))
            Controls::SetPadHeld(mapping.control, true);
        held[static_cast<int>(mapping.control)] = holds;
    }
    for (const Mapping& mapping : Mappings)
    {
        // A over the world with a window open holds the attack button itself (PlayPointer).
        if (mapping.control == PadButton::Attack && g_state.a.mode == Hold::Attack)
            continue;
        Controls::SetPadHeld(mapping.control, held[static_cast<int>(mapping.control)]);
    }
}

// Whether one of the game's windows (not the HUD's parts) or the skill list's grid is open: the pad's
// frame hops over it.
bool IsGameWindowOpen()
{
    if (g_pSkillList->IsSkillGridOpen())
        return true;
    for (int window = mu::ui::window::INTERFACE_BEGIN + 1; window < mu::ui::window::INTERFACE_END; ++window)
    {
        if (!g_pNewUISystem->IsImpossibleHideInterface(window) && g_pNewUISystem->IsVisible(window))
            return true;
    }
    return false;
}

// Menu tapped: with no window open the hero hub opens; in the hub, or over another window, the menu of
// every window.
void TapMenu(Clock::time_point now)
{
    if (UI::HUD::PadHub::IsOpen())
        UI::HUD::PadHub::Close();
    else if (!g_state.windowOpen)
    {
        UI::HUD::PadHub::Open(now);
        return;
    }
    TapMenuControl();
}

// Menu: a tap opens the hero hub or the menu of windows (TapMenu), held it opens the chat line (and the
// pad's keyboard with it).
void PlayMenuButton(const Presses& presses, Clock::time_point now)
{
    switch (ReadLongPress(g_state.menuPress, Button::Menu, presses, now, ChatHold))
    {
    case PressKind::Tap:
        TapMenu(now);
        break;
    case PressKind::Hold:
        // As Enter opens it (CChatInputBox): a tapped Enter did not reach it while Menu was held.
        if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX))
            g_pNewUISystem->Show(mu::ui::window::INTERFACE_CHATINPUTBOX);
        break;
    default:
        break;
    }
}

// Puts the pad's pointer on that NPC: the next frames click it, so the game walks there and talks as
// for a mouse's click. False with it not on the screen.
bool TalkToNpc(SDL_Window* window, int npc)
{
    float x = 0.f;
    float y = 0.f;
    if (npc < 0 || !GameLogic::Touch::NpcOnScreen(npc, x, y) || WindowWidth == 0 || WindowHeight == 0)
        return false;
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    Pointer::MoveTo(window, x * static_cast<float>(width) / static_cast<float>(WindowWidth),
                    y * static_cast<float>(height) / static_cast<float>(WindowHeight));
    g_state.npcClickStage = 1;
    return true;
}

// The same for the nearest NPC; false with none near.
bool TalkToNearestNpc(SDL_Window* window)
{
    return TalkToNpc(window, GameLogic::Touch::NearestNpc(NpcReach));
}

// The menu on a player (trade, party, follow, duel) beside them, as Alt and a right click on them
// opens it; false where it may not open (CQuickCommandWindow::IsMenuAllowedFor).
bool OpenPlayerMenu(int player)
{
    const CHARACTER* character = &CharactersClient[player];
    if (!mu::ui::window::CQuickCommandWindow::IsMenuAllowedFor(character))
        return false;
    float x = static_cast<float>(WindowWidth) * 0.5f;
    float y = static_cast<float>(WindowHeight) * 0.5f;
    (void)GameLogic::Touch::NpcOnScreen(player, x, y);
    g_pQuickCommand->OpenQuickCommandAt(character->ID, player, x, y);
    return true;
}

// No window open, with an NPC or a player the right stick chose: A talks to the NPC (the game walks
// there) or walks up to the player and opens their menu. The left stick or a window stops the walk.
void PlayMeet(SDL_Window* window, Presses& presses, Clock::time_point now)
{
    const int meet = GameLogic::Touch::MeetIndex();
    if (meet >= 0 && Take(presses, Button::A))
    {
        g_state.waitForRelease[static_cast<int>(Button::A)] = true;
        if (CharactersClient[meet].Object.Kind == KIND_NPC)
            TalkToNpc(window, meet);
        else if (mu::ui::window::CQuickCommandWindow::IsMenuAllowedFor(&CharactersClient[meet]))
        {
            g_state.menuWalk = meet;
            g_state.menuWalkUntil = now + PlayerMenuWalkTime;
        }
    }
    if (g_state.menuWalk < 0)
        return;
    if (g_state.menuWalk != meet || g_state.steering || now >= g_state.menuWalkUntil)
    {
        g_state.menuWalk = -1;
        return;
    }
    if (GameLogic::Touch::WalkUpTo(meet, PlayerMenuDistance))
    {
        g_state.menuWalk = -1;
        OpenPlayerMenu(meet);
    }
}

// The NPC click a frame at a time: the game reads the moved pointer first, then the button.
void FollowNpcClick(SDL_Window* window)
{
    if (g_state.npcClickStage == 1)
    {
        Pointer::SetButton(window, SDL_BUTTON_LEFT, true);
        g_state.npcClickStage = 2;
    }
    else if (g_state.npcClickStage == 2)
    {
        Pointer::SetButton(window, SDL_BUTTON_LEFT, false);
        g_state.npcClickStage = 0;
    }
}

// D-pad down: held, it gathers the drops around; with none there (and no window open) a press talks
// to the nearest NPC.
void PlayDpadDown(SDL_Window* window, const Presses& presses)
{
    const bool waiting = g_state.waitForRelease[static_cast<int>(Button::DpadDown)];
    const bool holds = HoldsControl(Button::DpadDown);
    const bool pressed = Pressed(presses, Button::DpadDown) && !waiting;
    if (pressed)
        g_state.dpadDownTalks =
            !g_state.windowOpen && !GameLogic::Touch::HasDropToPick() && TalkToNearestNpc(window);
    if (g_state.dpadDownTalks)
    {
        Controls::SetPadHeld(PadButton::PickUp, false);
        g_state.dpadDownTalks = holds;
        return;
    }
    if (!holds && pressed)
        Controls::SetPadHeld(PadButton::PickUp, true);
    Controls::SetPadHeld(PadButton::PickUp, holds);
}

// D-pad up (no game window): a tap turns the MU Helper on and off (its button, as Home), held half a
// second it opens the helper's settings (Z) in the world.
void PlayDpadUp(const Presses& presses, Clock::time_point now)
{
    switch (ReadLongPress(g_state.dpadUpPress, Button::DpadUp, presses, now, DpadHold))
    {
    case PressKind::Tap:
        Controls::SetPadHeld(PadButton::Auto, true);
        Controls::SetPadHeld(PadButton::Auto, false);
        break;
    case PressKind::Hold:
        if (!g_state.windowOpen)
            Core::Input::Touch::TapKey('Z');
        break;
    default:
        break;
    }
}

// D-pad left and right (no window open): the map (Tab) and the inventory (I), as a keyboard
// opens them (ghinzuro, 2026-11-03). Left held half a second: the Teleport window (M) instead.
void PlayDpadSides(const Presses& presses, Clock::time_point now)
{
    switch (ReadLongPress(g_state.dpadLeftPress, Button::DpadLeft, presses, now, DpadHold))
    {
    case PressKind::Tap:
        Core::Input::Touch::TapKey(VK_TAB);
        break;
    case PressKind::Hold:
        Core::Input::Touch::TapKey('M');
        break;
    default:
        break;
    }
    if (Pressed(presses, Button::DpadRight) && !g_state.waitForRelease[static_cast<int>(Button::DpadRight)])
        Core::Input::Touch::TapKey('I');
}

// A window opened or closed: what was held lets go and acts again only once let go, as the buttons
// change their meaning.
void SwitchWindowMode(SDL_Window* window, bool windowOpen, bool frameOpen)
{
    if (frameOpen && !g_state.frameOpen)
        UI::HUD::PadFrame::SnapSoon();
    else if (!frameOpen)
        UI::HUD::PadFrame::Reset();
    g_state.windowOpen = windowOpen;
    g_state.frameOpen = frameOpen;
    EndPress(window, g_state.a, {});
    Pointer::Release(window);
    ReleaseControls();
    g_state.npcClickStage = 0;
    g_state.menuWalk = -1;
    g_state.flicked = true;
    ForgetLongPresses();
    WaitForRelease();
}

void PlayWorld(SDL_Window* window, Presses presses, float seconds, Clock::time_point now)
{
    const bool dialogAnswers = DialogTakesAnswers(now);
    const bool aRested = now - g_state.aActive >= DialogAnswerDelay;
    if (Pressed(presses, Button::A) || Held(Button::A))
        g_state.aActive = now;
    UI::HUD::PadHub::Update(now);
    g_state.promptDrop = -1;
    if (Controls::IsMenuOpen())
    {
        PlayMenu(window, presses, now);
        return;
    }
    g_state.menuStepper.Reset();
    const bool frameOpen = IsGameWindowOpen();
    const bool windowOpen = frameOpen || UI::Dialogs::Modal::IsOpen();
    if (windowOpen != g_state.windowOpen || frameOpen != g_state.frameOpen)
        SwitchWindowMode(window, windowOpen, frameOpen);
    // Over a game window the left stick hops the frame: the character stands.
    if (frameOpen)
    {
        GameLogic::Touch::SetPadAim(0.f, 0.f);
        StopWalking();
    }
    else
        Walk(Pad::LeftStick());
    FollowNpcClick(window);
    if (dialogAnswers)
        AnswerDialog(presses, aRested);
    if (windowOpen)
    {
        TurnHelpPages(presses);
        TurnHubTabs(presses, now);
        if (frameOpen)
            PlayHeldItem(presses);
        if (!frameOpen)
            TurnWheelWithDpad(window, presses, now);
        PlayPointer(window, presses, seconds);
        if (frameOpen)
            UI::HUD::PadFrame::Update(window, presses, now);
    }
    else
    {
        PickTargets();
        PlayMeet(window, presses, now);
        // RB: whom the heals and buffs go to (ghinzuro, 2026-11-04).
        if (Pressed(presses, Button::RightShoulder) && !g_state.waitForRelease[static_cast<int>(Button::RightShoulder)])
            GameLogic::Touch::ChooseNextFriend();
        g_state.promptDrop = GameLogic::Touch::NearestDropToPick();
    }
    HoldMappedControls(presses);
    if (!frameOpen)
    {
        PlayDpadDown(window, presses);
        PlayDpadUp(presses, now);
        if (!windowOpen)
            PlayDpadSides(presses, now);
    }
    PlayMenuButton(presses, now);
    // The skill picker sets the skill buttons; without the controls on the screen, the game's list.
    if (g_state.leftStickClick.Pressed() && !Controls::OpenSkillPicker())
        g_pSkillList->OnCurrentSkillClick();
}

// Whether the character screen shows with nothing over it (no creation, menu or message box). A
// "Please wait" box has no buttons for the frame, and the bar stays shown under it: its keys wait too.
bool OnCharacterBar()
{
    return SceneFlag == CHARACTER_SCENE && g_CharSelMainWin.IsVisible() && !g_CharMakeWin.IsVisible()
           && !g_MsgWin.IsVisible() && !g_SysMenuWin.IsVisible();
}

// The next character on the screen from the chosen one, step 1 to the right, -1 to the left, going
// round; the first with none chosen.
void ChooseCharacter(int step)
{
    int slot = SelectedHero;
    for (int tries = 0; tries < MAX_CHARACTERS_PER_ACCOUNT; ++tries)
    {
        if (slot < 0)
            slot = step >= 0 ? 0 : MAX_CHARACTERS_PER_ACCOUNT - 1;
        else
            slot = (slot + step + MAX_CHARACTERS_PER_ACCOUNT) % MAX_CHARACTERS_PER_ACCOUNT;
        const wchar_t* name = Scenes::CharacterNameInSlot(slot);
        if (name != nullptr && name[0] != L'\0')
        {
            SelectedHero = slot;
            g_CharSelMainWin.UpdateDisplay();
            return;
        }
    }
}

// The character screen with nothing over it: LB and RB or the d-pad choose the character (the first
// is chosen at once), A starts the game with it, X creates one, Y deletes the chosen one, Menu opens
// the menu, B is Esc.
void PlayCharacters(const Presses& presses)
{
    if (SelectedHero < 0)
        ChooseCharacter(1);
    if (Pressed(presses, Button::LeftShoulder) || Pressed(presses, Button::DpadLeft))
        ChooseCharacter(-1);
    if (Pressed(presses, Button::RightShoulder) || Pressed(presses, Button::DpadRight))
        ChooseCharacter(1);
    if (Pressed(presses, Button::A) && SelectedHero >= 0)
        Scenes::StartGameWithSlot(SelectedHero);
    else if (Pressed(presses, Button::X))
        g_CharSelMainWin.RmlClickCreate();
    else if (Pressed(presses, Button::Y))
        g_CharSelMainWin.RmlClickDelete();
    else if (Pressed(presses, Button::Menu))
        g_CharSelMainWin.RmlClickMenu();
    else if (Pressed(presses, Button::B))
        Core::Input::Touch::TapKey(VK_ESCAPE);
}

// The login, server and character screens. Over their controls the frame works as over the windows:
// the d-pad and the left stick hop it, A presses there; without any (the character screen bare) the
// face buttons work the characters (PlayCharacters). B is Esc, Menu Enter.
void PlayScreens(SDL_Window* window, Presses presses, float seconds, Clock::time_point now)
{
    const bool frameOpen = !UI::HUD::PadTargets::Collect().empty();
    const bool onCharacters = !frameOpen && OnCharacterBar();
    if (frameOpen != g_state.frameOpen || onCharacters != g_state.onCharacters)
    {
        if (frameOpen)
            UI::HUD::PadFrame::SnapSoon();
        else
            UI::HUD::PadFrame::Reset();
        g_state.frameOpen = frameOpen;
        g_state.onCharacters = onCharacters;
        Pointer::Release(window);
        WaitForRelease();
    }
    if (onCharacters)
    {
        PlayCharacters(presses);
        return;
    }
    const Pad::StickPosition left = Pad::LeftStick();
    const Pad::StickPosition right = Pad::RightStick();
    if (frameOpen)
    {
        Pointer::Move(window, right.x, right.y, seconds, true);
        UI::HUD::PadFrame::Update(window, presses, now);
    }
    else
    {
        const bool leftFurther = std::hypot(left.x, left.y) >= std::hypot(right.x, right.y);
        const Pad::StickPosition stick = leftFurther ? left : right;
        Pointer::Move(window, stick.x, stick.y, seconds, false);
    }
    const bool held = HoldsControl(Button::A) || HoldsControl(Button::RightTrigger);
    // A click shorter than a frame still clicks once.
    if (!held && (Pressed(presses, Button::A) || Pressed(presses, Button::RightTrigger)))
        Pointer::SetButton(window, SDL_BUTTON_LEFT, true);
    Pointer::SetButton(window, SDL_BUTTON_LEFT, held);
    // On the login screen a press may bring up new buttons (a group's servers, the login fields): the
    // frame goes onto them, not stays on the button pressed.
    if (frameOpen && SceneFlag == LOG_IN_SCENE && Pressed(presses, Button::A))
        UI::HUD::PadFrame::SnapOnNewWindow();
    if (Pressed(presses, Button::B))
        Core::Input::Touch::TapKey(VK_ESCAPE);
    if (Pressed(presses, Button::Menu))
        Core::Input::Touch::TapKey(VK_RETURN);
}

// The pad takes the pointer where the mouse left it, so the window or the item under it stays under
// it. The first time, and with the pointer off the window (a finger lifted moves it off), it starts
// in the middle, over the character.
void TakePointer(SDL_Window* window)
{
    float x = 0.f;
    float y = 0.f;
    PointerPosition(x, y);
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    // SDL may keep a warp off the window at its corner.
    const bool offWindow = (x <= 0.f && y <= 0.f) || x < 0.f || y < 0.f || x >= static_cast<float>(width) ||
                           y >= static_cast<float>(height);
    if (!g_state.pointerTaken || offWindow)
        Pointer::Centre(window);
    g_state.pointerTaken = true;
}

void SwitchUse(SDL_Window* window, bool inUse)
{
    g_state.inUse = inUse;
    if (inUse && window != nullptr)
        TakePointer(window);
    else if (!inUse)
        ReleaseAll(window);
    UI::Mobile::SetGamepadInUse(inUse);
#ifdef __EMSCRIPTEN__
    // The pad's own keyboard types (UI/HUD/PadKeyboard): the phone's stays down meanwhile.
    mu::web::HoldBackKeyboard(inUse);
#endif
}

// The on-screen keyboard has the pad: nothing else acts, and what was held acts again only once let
// go, so the B that closed it does not go on to press Back.
void HandToKeyboard(SDL_Window* window)
{
    if (!g_state.typing)
        ReleaseAll(window);
    g_state.typing = true;
    WaitForRelease();
}
// The pad's frame of play: reads the pad and acts (below).
void Play();

// The pad shakes as the hero takes a hard blow: a share of life and shield together this big or more.
constexpr float FeltBlowShare = 0.04f;
// A blow of this share or more shakes at full strength.
constexpr float FullBlowShare = 0.3f;
constexpr int BlowMilliseconds = 160;
constexpr int DeathMilliseconds = 600;

struct Health
{
    bool known = false;
    bool dead = false;
    long long life = 0;
    long long shield = 0;
};
Health g_health;

// Compares the hero's life and shield with the last frame's, while the pad plays the world: a blow
// shakes it as hard as it hurt, the death long. Out of the world (or the pad put down) it only forgets
// them, so a heal or a map change since is no blow.
void FeelBlows(bool playing)
{
    if (!playing || Hero == nullptr || CharacterAttribute == nullptr)
    {
        g_health.known = false;
        return;
    }
    Health now;
    now.known = true;
    now.dead = Hero->Dead != 0;
    now.life = CharacterAttribute->Life;
    now.shield = CharacterAttribute->Shield;
    const Health before = std::exchange(g_health, now);
    if (!before.known)
        return;
    if (now.dead && !before.dead)
    {
        Pad::Rumble(1.f, 0.6f, DeathMilliseconds);
        return;
    }
    const long long most = static_cast<long long>(CharacterAttribute->LifeMax) + CharacterAttribute->ShieldMax;
    const long long lost = std::max(before.life - now.life, 0LL) + std::max(before.shield - now.shield, 0LL);
    if (now.dead || most <= 0 || lost <= 0)
        return;
    const float share = static_cast<float>(lost) / static_cast<float>(most);
    if (share < FeltBlowShare)
        return;
    const float strength = std::min(share / FullBlowShare, 1.f);
    Pad::Rumble(0.25f + 0.75f * strength, 0.4f + 0.6f * strength, BlowMilliseconds);
}

// What the overlay shows this frame: the frame, the hub's tabs, the legend over a game window, the
// pickup prompt under the drop d-pad down would take.
UI::HUD::PadOverlay::View OverlayView(bool playing)
{
    UI::HUD::PadOverlay::View view;
    if (!playing)
        return view;
    // Out of the world: the frame over the screens' controls, the character screen's keys.
    if (!g_state.inWorld)
    {
        view.frame = g_state.frameOpen && UI::HUD::PadFrame::Box(view.frameBox);
        view.characters = g_state.onCharacters;
        return view;
    }
    // A dialog over the windows says its own answers: the tabs and the legend make room for it.
    const bool dialog = g_state.frameOpen && UI::HUD::PadTargets::OnDialog();
    // The menu of windows says its own keys and has its own highlight: the frame and the legend wait under it.
    const bool menu = Controls::IsMenuOpen();
    view.hub = UI::HUD::PadHub::IsOpen() && !dialog;
    // The skill picker says its own keys too.
    view.legend = g_state.frameOpen && !dialog && !menu && !UI::HUD::PadHub::IsOnSkills();
    view.frame = g_state.frameOpen && !menu && UI::HUD::PadFrame::Box(view.frameBox);
    using Legend = UI::HUD::PadOverlay::View::Legend;
    if (UI::Inventory::HasPickedItem())
        view.legendSays = Legend::Holding;
    else if (view.frame && !UI::HUD::PadFrame::OnItemPlace())
        view.legendSays = Legend::Button;
    else if (view.frame && UI::HUD::PadFrame::OnEmptyCell())
        view.legendSays = Legend::Empty;
    else if (view.frame && UI::HUD::PadFrame::OnWornPlace())
        view.legendSays = Legend::Worn;
    else if (view.frame && UI::HUD::PadFrame::OnGoods())
        view.legendSays = Legend::Goods;
    else if (view.frame && UI::HUD::PadFrame::OnMixItem())
        view.legendSays = Legend::Crafting;
    else if (view.frame)
    {
        using Does = mu::ui::window::CInventoryActionController::BagRightClick;
        const Does does = mu::ui::window::CInventoryActionController::WhatBagRightClickDoes();
        if (does == Does::Sells)
            view.legendSays = Legend::Sell;
        else if (does == Does::Moves)
            view.legendSays = Legend::Move;
        else if (does == Does::Uses)
            view.legendSays = Legend::Use;
        else if (does == Does::Keeps)
            view.legendSays = Legend::Keep;
    }
    float x = 0.f;
    float y = 0.f;
    const Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    const std::optional<float> windowsLeft = UI::HUD::PadTargets::WindowsLeft();
    if (g_state.frameOpen && windowsLeft && context != nullptr)
        view.topRight = std::max(static_cast<float>(context->GetDimensions().x) - *windowsLeft, 0.f);
    using Prompt = UI::HUD::PadOverlay::View::Prompt;
    // The NPC or player chosen says what A does, under their feet; else the drop d-pad down takes.
    const int meet = g_state.windowOpen ? -1 : GameLogic::Touch::MeetIndex();
    bool prompted = false;
    if (meet >= 0 && GameLogic::Touch::FeetOnScreen(meet, x, y))
    {
        prompted = true;
        view.promptSays = CharactersClient[meet].Object.Kind == KIND_NPC ? Prompt::Talk : Prompt::Menu;
    }
    else if (g_state.promptDrop >= 0)
        prompted = GameLogic::Touch::DropOnScreen(g_state.promptDrop, x, y);
    if (prompted && context != nullptr && WindowWidth > 0)
    {
        // The camera's point is in the game's window pixels, the overlay's in the context's.
        const float scale = static_cast<float>(context->GetDimensions().x) / static_cast<float>(WindowWidth);
        view.prompt = true;
        view.promptX = x * scale;
        view.promptY = y * scale;
    }
    return view;
}
} // namespace

namespace UI::HUD::GamepadControls
{
bool PlaysPointer()
{
    return g_state.inUse && (!g_state.inWorld || g_state.windowOpen);
}

bool HidesPointer()
{
    if (!g_state.inUse)
        return false;
    // The pad's keyboard has the pad: the pointer, left where the field's window was pressed, would sit
    // on its keys.
    if (g_state.typing)
        return true;
    PadTargetPick::Box box;
    if (!g_state.inWorld)
        return g_state.onCharacters || (g_state.frameOpen && UI::HUD::PadFrame::Box(box));
    return !g_state.windowOpen || (g_state.frameOpen && UI::HUD::PadFrame::Box(box));
}

int PromptDrop()
{
    return g_state.inUse && g_state.inWorld ? g_state.promptDrop : -1;
}

int PromptMeet()
{
    return g_state.inUse && g_state.inWorld && !g_state.windowOpen ? GameLogic::Touch::MeetIndex() : -1;
}

void Update()
{
    Play();
    const bool playing = g_state.inUse && !g_state.typing;
    FeelBlows(g_state.inUse && g_state.inWorld && IsInWorld());
    UI::HUD::PadOverlay::Update(OverlayView(playing));
}
} // namespace UI::HUD::GamepadControls

namespace
{
void Play()
{
    SDL_Window* window = Core::Platform::GameWindow();
    const Clock::time_point now = Clock::now();
    const float seconds = FrameSeconds(now);
    Pad::Poll();
    const bool inUse = Pad::IsConnected() && Pad::WasUsedLast();
    if (inUse != g_state.inUse)
        SwitchUse(window, inUse);
    const Presses presses = TakePresses();
    if (inUse)
        UpdateStickClicks(presses, now);
    const bool typing = UI::HUD::PadKeyboard::Update(inUse && window != nullptr, presses);
    if (!inUse || window == nullptr)
        return;
    if (typing)
    {
        HandToKeyboard(window);
        return;
    }
    g_state.typing = false;

    const bool inWorld = IsInWorld();
    // A map loading in the world (a gate, a warp): the pad rests, no frame over the HUD meanwhile.
    const bool loadingMap = SceneFlag == MAIN_SCENE && !inWorld;
    UI::HUD::PadTargets::SetScreens(!inWorld && !loadingMap);
    if (inWorld != g_state.inWorld)
    {
        ReleaseForWorldChange(window);
        g_state.inWorld = inWorld;
    }
    if (loadingMap)
    {
        if (g_state.frameOpen || g_state.onCharacters)
            ReleaseForWorldChange(window);
        return;
    }
    if (!inWorld)
    {
        PlayScreens(window, presses, seconds, now);
        return;
    }
    TellOnce();
    PlayWorld(window, presses, seconds, now);
}
} // namespace
