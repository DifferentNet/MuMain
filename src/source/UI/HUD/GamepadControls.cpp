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
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Touch/TouchActions.h"
#include "I18N/All.h"
#include "Scenes/SceneCore.h"
#include "UI/Chat/MessageType.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/HelpWindow.h"
#include "UI/Dialogs/ModalDialogs.h"
#include "UI/HUD/MainFrameWindow.h"
#include "UI/HUD/PadKeyboard.h"
#include "UI/HUD/SkillList.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Mobile/InterfaceMode.h"

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebKeyboard.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

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
// A dialog takes the pad's answer (A, Menu) once it has been up this long: a press meant for the
// fight as a trade or party request came up does not accept it.
constexpr auto DialogAnswerDelay = std::chrono::milliseconds(1000);

// The pad buttons that press a control as long as they are held.
struct Mapping
{
    Button pad;
    PadButton control;
};
constexpr Mapping Mappings[] = {
    {Button::B, PadButton::Back},
    {Button::X, PadButton::Skill0},
    {Button::Y, PadButton::Skill1},
    {Button::RightShoulder, PadButton::Skill2},
    {Button::LeftTrigger, PadButton::Skill3},
    {Button::DpadDown, PadButton::Skill4},
    {Button::LeftShoulder, PadButton::PickUp},
    {Button::DpadUp, PadButton::Auto},
    {Button::DpadLeft, PadButton::PotionQ},
    {Button::DpadRight, PadButton::PotionW},
    {Button::View, PadButton::Map},
    {Button::Menu, PadButton::Menu},
};

// What A or RT does while held, decided as it is pressed.
enum class Hold : std::uint8_t
{
    None,
    // A: the attack button.
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

struct State
{
    bool inUse = false;
    bool inWorld = false;
    // The hint on how to play with a pad, once a session.
    bool told = false;
    std::array<std::uint32_t, Pad::ButtonCount> seen{};
    // Buttons held while the menu was open: they act in the world only once let go, so the B that
    // closed the menu does not go on to press Back.
    std::array<bool, Pad::ButtonCount> waitForRelease{};
    PointerPress a;
    PointerPress rightTrigger;
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
        StopWalking();
        return;
    }
    GameLogic::Touch::Steer(stick.x * StickToScreen, stick.y * StickToScreen);
    g_state.steering = true;
}

void ReleaseControls()
{
    for (const Mapping& mapping : Mappings)
        Controls::SetPadHeld(mapping.control, false);
}

// Whether a button holds its control: held, and let go since the menu last had it.
bool HoldsControl(Button button)
{
    bool& waiting = g_state.waitForRelease[static_cast<int>(button)];
    if (waiting && !Held(button))
        waiting = false;
    return Held(button) && !waiting;
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
    EndPress(window, g_state.rightTrigger, {});
    ReleaseControls();
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
}

void TellOnce()
{
    if (g_state.told)
        return;
    g_state.told = true;
    g_pSystemLogBox->AddText(I18N::Game::PadConnected, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

// The menu open: the stick and the d-pad move its highlight, A opens the item, B closes the menu,
// Menu shuts it as it opened it. Nothing else acts: the pointer and its buttons rest too.
void PlayMenu(SDL_Window* window, const Presses& presses, Clock::time_point now)
{
    StopWalking();
    EndPress(window, g_state.a, g_state.rightTrigger);
    EndPress(window, g_state.rightTrigger, g_state.a);
    Pointer::SetButton(window, SDL_BUTTON_RIGHT, false);
    for (const Mapping& mapping : Mappings)
        Controls::SetPadHeld(mapping.control, mapping.control == PadButton::Menu && Held(Button::Menu));
    WaitForRelease();

    Controls::MoveMenuFocus(0, 0);
    const Pad::Step step = g_state.menuStepper.Take(presses, now);
    if (step.dx != 0 || step.dy != 0)
        Controls::MoveMenuFocus(step.dx, step.dy);
    if (Pressed(presses, Button::A))
        Controls::ActivateMenuFocus();
    if (Pressed(presses, Button::B))
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

// RS: the right mouse button at the pointer, down as long as the stick's button (a skill held on a
// monster); a click shorter than a frame still clicks once.
void PressRightButton(SDL_Window* window)
{
    const Pad::StickClick& click = g_state.rightStickClick;
    const bool held = click.Held() && HoldsControl(Button::RightStick);
    if (click.Pressed() && !held)
        Pointer::SetButton(window, SDL_BUTTON_RIGHT, true);
    Pointer::SetButton(window, SDL_BUTTON_RIGHT, held);
}

// The right stick moves the pointer, RT is its left button (a click, a drag), RS its right button.
void PlayPointer(SDL_Window* window, const Presses& presses, float seconds)
{
    const Pad::StickPosition pointer = Pad::RightStick();
    Pointer::Move(window, pointer.x, pointer.y, seconds, IsPointerOverUi());
    FollowPress(window, g_state.rightTrigger, g_state.a, Pressed(presses, Button::RightTrigger),
                Held(Button::RightTrigger), false);
    PressRightButton(window);
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
void AnswerDialog(Presses& presses)
{
    if (Take(presses, Button::Menu))
    {
        Core::Input::Touch::TapKey(VK_RETURN);
        // Still held as the dialog goes, it does not go on to open the menu.
        g_state.waitForRelease[static_cast<int>(Button::Menu)] = true;
    }
    if (Pressed(presses, Button::A) && g_state.a.mode == Hold::None && !IsPointerOnButton())
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

// The buttons that press a control hold it while held. A tap shorter than a frame (a quick one on a
// slow phone, the press that woke the pad in a browser) still presses it once.
void HoldMappedControls(const Presses& presses)
{
    for (const Mapping& mapping : Mappings)
    {
        const bool holds = HoldsControl(mapping.pad);
        if (!holds && Pressed(presses, mapping.pad))
            Controls::SetPadHeld(mapping.control, true);
        Controls::SetPadHeld(mapping.control, holds);
    }
}

void PlayWorld(SDL_Window* window, Presses presses, float seconds, Clock::time_point now)
{
    const bool dialogAnswers = DialogTakesAnswers(now);
    if (Controls::IsMenuOpen())
    {
        PlayMenu(window, presses, now);
        return;
    }
    g_state.menuStepper.Reset();
    PlayPointer(window, presses, seconds);
    Walk(Pad::LeftStick());
    TurnHelpPages(presses);
    if (dialogAnswers)
        AnswerDialog(presses);
    FollowPress(window, g_state.a, g_state.rightTrigger, Take(presses, Button::A), Held(Button::A), true);
    HoldMappedControls(presses);
    // The skill picker sets the skill buttons; without the controls on the screen, the game's list.
    if (g_state.leftStickClick.Pressed() && !Controls::OpenSkillPicker())
        g_pSkillList->OnCurrentSkillClick();
}

// The login, server and character screens: the pointer, its clicks, Esc and Enter.
void PlayScreens(SDL_Window* window, const Presses& presses, float seconds)
{
    const Pad::StickPosition left = Pad::LeftStick();
    const Pad::StickPosition right = Pad::RightStick();
    const bool leftFurther = std::hypot(left.x, left.y) >= std::hypot(right.x, right.y);
    const Pad::StickPosition stick = leftFurther ? left : right;
    Pointer::Move(window, stick.x, stick.y, seconds, false);
    const bool held = HoldsControl(Button::A) || HoldsControl(Button::RightTrigger);
    // A click shorter than a frame still clicks once.
    if (!held && (Pressed(presses, Button::A) || Pressed(presses, Button::RightTrigger)))
        Pointer::SetButton(window, SDL_BUTTON_LEFT, true);
    Pointer::SetButton(window, SDL_BUTTON_LEFT, held);
    if (Pressed(presses, Button::B))
        Core::Input::Touch::TapKey(VK_ESCAPE);
    if (Pressed(presses, Button::Menu))
        Core::Input::Touch::TapKey(VK_RETURN);
}

void SwitchUse(SDL_Window* window, bool inUse)
{
    g_state.inUse = inUse;
    if (inUse)
        Pointer::Centre(window);
    else
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
} // namespace

namespace UI::HUD::GamepadControls
{
void Update()
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
    if (inWorld != g_state.inWorld)
    {
        ReleaseForWorldChange(window);
        g_state.inWorld = inWorld;
    }
    if (!inWorld)
    {
        PlayScreens(window, presses, seconds);
        return;
    }
    TellOnce();
    PlayWorld(window, presses, seconds, now);
}
} // namespace UI::HUD::GamepadControls
