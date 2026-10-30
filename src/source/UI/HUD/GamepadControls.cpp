#include "stdafx.h"
#include "UI/HUD/GamepadControls.h"

#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadStepper.h"
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
    bool steering = false;
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
        Core::Input::Gamepad::Pointer::SetButton(window, SDL_BUTTON_LEFT, true);
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
            Core::Input::Gamepad::Pointer::SetButton(window, SDL_BUTTON_LEFT, false);
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

void Walk(Pad::StickPosition stick)
{
    if (stick.x == 0.f && stick.y == 0.f)
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

// Lets go of everything the pad holds: it was put down, or the world was left.
void ReleaseAll(SDL_Window* window)
{
    EndPress(window, g_state.a, {});
    EndPress(window, g_state.rightTrigger, {});
    ReleaseControls();
    Controls::ReleasePad();
    Core::Input::Gamepad::Pointer::Release(window);
    StopWalking();
    g_state.menuStepper.Reset();
}

void TellOnce()
{
    if (g_state.told)
        return;
    g_state.told = true;
    g_pSystemLogBox->AddText(I18N::Game::PadConnected, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

// The menu open: the stick and the d-pad move its highlight, A opens the item, B closes the menu,
// Menu shuts it as it opened it. Nothing else acts.
void PlayMenu(SDL_Window* window, const Presses& presses, Clock::time_point now)
{
    StopWalking();
    EndPress(window, g_state.a, g_state.rightTrigger);
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

void PlayWorld(SDL_Window* window, const Presses& presses, float seconds, Clock::time_point now)
{
    const Pad::StickPosition pointer = Pad::RightStick();
    Core::Input::Gamepad::Pointer::Move(window, pointer.x, pointer.y, seconds, IsPointerOverUi());
    FollowPress(window, g_state.rightTrigger, g_state.a, Pressed(presses, Button::RightTrigger),
                Held(Button::RightTrigger), false);
    Core::Input::Gamepad::Pointer::SetButton(window, SDL_BUTTON_RIGHT, Held(Button::RightStick));

    if (Controls::IsMenuOpen())
    {
        PlayMenu(window, presses, now);
        return;
    }
    g_state.menuStepper.Reset();
    Walk(Pad::LeftStick());
    FollowPress(window, g_state.a, g_state.rightTrigger, Pressed(presses, Button::A), Held(Button::A), true);
    for (const Mapping& mapping : Mappings)
        Controls::SetPadHeld(mapping.control, HoldsControl(mapping.pad));
    if (Pressed(presses, Button::LeftStick))
        g_pSkillList->OnCurrentSkillClick();
}

// The login, server and character screens: the pointer, its clicks, Esc and Enter.
void PlayScreens(SDL_Window* window, const Presses& presses, float seconds)
{
    const Pad::StickPosition left = Pad::LeftStick();
    const Pad::StickPosition right = Pad::RightStick();
    const bool leftFurther = std::hypot(left.x, left.y) >= std::hypot(right.x, right.y);
    const Pad::StickPosition stick = leftFurther ? left : right;
    Core::Input::Gamepad::Pointer::Move(window, stick.x, stick.y, seconds, false);
    Core::Input::Gamepad::Pointer::SetButton(window, SDL_BUTTON_LEFT,
                                             HoldsControl(Button::A) || HoldsControl(Button::RightTrigger));
    if (Pressed(presses, Button::B))
        Core::Input::Touch::TapKey(VK_ESCAPE);
    if (Pressed(presses, Button::Menu))
        Core::Input::Touch::TapKey(VK_RETURN);
}

void SwitchUse(SDL_Window* window, bool inUse)
{
    g_state.inUse = inUse;
    if (inUse)
        Core::Input::Gamepad::Pointer::Centre(window);
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
        ReleaseAll(window);
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
