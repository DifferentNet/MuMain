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

// A pad's button pressed since the last frame, or a stick out of its dead zone.
bool PadUsed()
{
    std::uint32_t presses = 0;
    for (int button = 0; button < Pad::ButtonCount; ++button)
        presses += Pad::GetButton(static_cast<Pad::Button>(button)).presses;
    const bool pressed = presses != g_page.padPresses;
    g_page.padPresses = presses;
    const Pad::StickPosition left = Pad::LeftStick();
    const Pad::StickPosition right = Pad::RightStick();
    return pressed || left.x != 0.f || left.y != 0.f || right.x != 0.f || right.y != 0.f;
}

bool IsIdle(Clock::time_point now)
{
    // Both read every frame, so neither keeps an old input for the next one.
    const bool page = mu::web::TakePageInputs() > 0;
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
#else
void UI::Mobile::PhonePage::Update() {}
#endif
