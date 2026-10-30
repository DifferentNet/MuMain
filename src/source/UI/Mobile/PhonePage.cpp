#include "stdafx.h"
#include "UI/Mobile/PhonePage.h"

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebScreen.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Scenes/SceneCore.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Mobile/InterfaceMode.h"

#include <chrono>

namespace
{
// The wake lock is asked for again this often while wanted: a browser may refuse it for a while (a
// battery saver) and give it later.
constexpr auto AskAgainEvery = std::chrono::seconds(5);

struct PageState
{
    bool backArmed = false;
    bool screenOn = false;
    std::chrono::steady_clock::time_point askedAt{};
};
PageState g_page;

bool IsPlayedByHand()
{
    return UI::Mobile::IsPhoneView() || UI::Mobile::IsTouchControls();
}

void SyncScreen()
{
    const bool inWorld = SceneFlag == MAIN_SCENE && LoadingWorld < 30 && Hero != nullptr;
    const bool wanted = inWorld && IsPlayedByHand();
    const auto now = std::chrono::steady_clock::now();
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
