#include "stdafx.h"
#include "UI/Mobile/PhonePage.h"

#ifdef __EMSCRIPTEN__
#include "Core/Input/GamepadInput.h"
#include "Core/Platform/web/WebScreen.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "MUHelper/MuHelper.h"
#include "Scenes/SceneCore.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Mobile/InterfaceMode.h"

#include <chrono>
#include <cstdint>

namespace
{
using Clock = std::chrono::steady_clock;
namespace Pad = Core::Input::Gamepad;

// The wake lock is asked for again this often while wanted: a browser may refuse it for a while (a
// battery saver) and give it later.
constexpr auto AskAgainEvery = std::chrono::seconds(5);
// A phone nobody touches lets its screen dim after this long, as it would without the game: one
// left on a table must not burn its battery. The next touch, key or pad press keeps it on again.
// While the MU Helper hunts it stays on: that is the player watching, not away.
constexpr auto IdleBeforeDim = std::chrono::minutes(5);

struct PageState
{
    bool backArmed = false;
    bool screenOn = false;
    Clock::time_point askedAt{};
    // The last input, none yet while it is the clock's start.
    Clock::time_point inputAt{};
    // The pad's presses added up, as last seen (the counters only grow).
    std::uint32_t padPresses = 0;
};
PageState g_page;

bool IsPlayedByHand()
{
    return UI::Mobile::IsPhoneView() || UI::Mobile::IsTouchControls();
}

// A pad's button pressed since the last frame or still held (a trigger pulled past half way counts
// as one), or a stick out of its dead zone. The pad lets go of everything when the page loses the
// focus or the pad goes (Core::Input::Gamepad), so nothing stays held from there.
bool PadUsed()
{
    std::uint32_t presses = 0;
    bool held = false;
    for (int button = 0; button < Pad::ButtonCount; ++button)
    {
        const Pad::ButtonState& state = Pad::GetButton(static_cast<Pad::Button>(button));
        presses += state.presses;
        held = held || state.held;
    }
    const bool pressed = presses != g_page.padPresses;
    g_page.padPresses = presses;
    const Pad::StickPosition left = Pad::LeftStick();
    const Pad::StickPosition right = Pad::RightStick();
    return pressed || held || left.x != 0.f || left.y != 0.f || right.x != 0.f || right.y != 0.f;
}

bool IsIdle(Clock::time_point now)
{
    // Both read every frame, so neither keeps an old input for the next one. A control held down is
    // input all along, however still: a finger on Attack, a skill or the joystick, a pad button.
    const bool page = mu::web::TakePageInputs() > 0 || mu::web::IsPageContactDown();
    const bool pad = PadUsed();
    if (page || pad || g_page.inputAt == Clock::time_point{})
        g_page.inputAt = now;
    return now - g_page.inputAt >= IdleBeforeDim;
}

void SyncScreen()
{
    const auto now = Clock::now();
    const bool inWorld = SceneFlag == MAIN_SCENE && LoadingWorld < 30 && Hero != nullptr;
    const bool awake = !IsIdle(now) || MUHelper::g_MuHelper.IsActive();
    const bool wanted = inWorld && IsPlayedByHand() && awake;
    if (wanted == g_page.screenOn && (!wanted || now - g_page.askedAt < AskAgainEvery))
        return;
    g_page.screenOn = wanted;
    g_page.askedAt = now;
    mu::web::KeepScreenOn(wanted);
}

void SyncBack()
{
    const bool armed = IsPlayedByHand();
    if (armed != g_page.backArmed)
    {
        g_page.backArmed = armed;
        mu::web::ArmBackGesture(armed);
    }
    // Several steps back in one frame are one Back: a window closes at a time.
    if (mu::web::TakeBackPresses() > 0 && armed)
        UI::HUD::TouchControls::PressBack();
}
} // namespace

void UI::Mobile::PhonePage::Update()
{
    SyncScreen();
    SyncBack();
}

double UI::Mobile::PhonePage::SecondsSinceInput()
{
    if (g_page.inputAt == Clock::time_point{})
        return 0.0;
    return std::chrono::duration<double>(Clock::now() - g_page.inputAt).count();
}
#else
void UI::Mobile::PhonePage::Update() {}

double UI::Mobile::PhonePage::SecondsSinceInput()
{
    return 0.0;
}
#endif
