#include "stdafx.h"
#include "UI/Mobile/InterfaceMode.h"

#include "Core/Input/TouchInput.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Placement/WindowPlacement.h"

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebDevice.h"
#include "Core/Platform/web/WebPageView.h"
#endif

#include <RmlUi/Core/Context.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace
{
// The `@media (theme: ...)` names: the phone layout, and the phone layout with the touch controls,
// where the PC's bottom bar gives way to theirs (main_frame.rcss).
constexpr char MobileThemeFlag[] = "mobile";
constexpr char MobileHudThemeFlag[] = "mobile-hud";
// A phone or tablet in the browser, whichever layout is picked: the page keeps its own menu button in
// the top right corner there, over whatever the game draws under it (mini_map.rcss).
constexpr char HandheldThemeFlag[] = "handheld";

// config.ini words, in the enums' order.
constexpr std::array<std::wstring_view, UI::Mobile::ViewChoiceCount> ViewWords = {L"auto", L"pc", L"phone"};
constexpr std::array<std::wstring_view, UI::Mobile::ControlChoiceCount> ControlWords = {L"auto", L"mouse", L"touch"};

bool g_phoneView = false;
bool g_touchControls = false;
bool g_gamepadInUse = false;

// A word config.ini does not know (a hand edit) reads as the first, "auto".
template <typename Choice, std::size_t Count>
Choice FromWord(const std::wstring& word, const std::array<std::wstring_view, Count>& words)
{
    for (std::size_t index = 0; index < Count; ++index)
    {
        if (word == words[index])
            return static_cast<Choice>(index);
    }
    return static_cast<Choice>(0);
}

// The style flags every document sees; the layout follows next frame (workspace regions move,
// windows resize).
void ApplyThemeFlags()
{
    if (Rml::Context* context = RmlUiRuntime::Instance().GetContext())
    {
        context->ActivateTheme(MobileThemeFlag, g_phoneView);
        context->ActivateTheme(MobileHudThemeFlag, UI::Mobile::ReplacesPcHud());
        context->ActivateTheme(HandheldThemeFlag, UI::Mobile::IsHandheld());
    }
    UI::Placement::InvalidateStyles();
}

template <typename Choice, std::size_t Count>
std::wstring ToWord(Choice choice, const std::array<std::wstring_view, Count>& words)
{
    const auto index = static_cast<std::size_t>(choice);
    return std::wstring(index < Count ? words[index] : words[0]);
}
} // namespace

UI::Mobile::ViewChoice UI::Mobile::GetViewChoice()
{
    return FromWord<ViewChoice>(GameConfig::GetInstance().GetInterfaceView(), ViewWords);
}

UI::Mobile::ControlChoice UI::Mobile::GetControlChoice()
{
    return FromWord<ControlChoice>(GameConfig::GetInstance().GetControls(), ControlWords);
}

void UI::Mobile::SetViewChoice(ViewChoice choice)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetInterfaceView(ToWord(choice, ViewWords));
    config.Save();
    Apply();
}

void UI::Mobile::SetControlChoice(ControlChoice choice)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetControls(ToWord(choice, ControlWords));
    config.Save();
    Apply();
}

bool UI::Mobile::IsHandheld()
{
#ifdef __EMSCRIPTEN__
    return mu::web::IsTouchDevice();
#else
    return false;
#endif
}

bool UI::Mobile::IsPhoneView()
{
    return g_phoneView;
}

bool UI::Mobile::IsTouchControls()
{
    return g_touchControls;
}

void UI::Mobile::SetGamepadInUse(bool inUse)
{
    if (inUse == g_gamepadInUse)
        return;
    g_gamepadInUse = inUse;
    ApplyThemeFlags();
}

bool UI::Mobile::IsGamepadInUse()
{
    return g_gamepadInUse;
}

bool UI::Mobile::ShowsControls()
{
    return g_touchControls || g_gamepadInUse;
}

bool UI::Mobile::ReplacesPcHud()
{
    return g_phoneView && ShowsControls();
}

void UI::Mobile::Apply()
{
    const bool handheld = IsHandheld();
    const ViewChoice view = GetViewChoice();
    const ControlChoice controls = GetControlChoice();
    // A phone or tablet always plays the phone layout with the fingers; the saved choices stay in
    // config.ini for a computer.
    g_phoneView = handheld || view == ViewChoice::Phone;
    g_touchControls = handheld || controls == ControlChoice::Touch;

    Core::Input::Touch::SetEnabled(g_touchControls);
    ApplyThemeFlags();
#ifdef __EMSCRIPTEN__
    mu::web::ShowPageView(static_cast<int>(view), g_phoneView);
#endif
}

void UI::Mobile::FollowPage()
{
#ifdef __EMSCRIPTEN__
    const int request = mu::web::TakePageViewRequest();
    if (request >= 0 && request < ViewChoiceCount)
        SetViewChoice(static_cast<ViewChoice>(request));
#endif
}
