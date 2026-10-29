#include "stdafx.h"
#include "UI/Mobile/InterfaceMode.h"

#include "Core/Input/TouchInput.h"
#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Placement/WindowPlacement.h"

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebDevice.h"
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

// config.ini words, in the enums' order.
constexpr std::array<std::wstring_view, UI::Mobile::ViewChoiceCount> ViewWords = {L"auto", L"pc", L"phone"};
constexpr std::array<std::wstring_view, UI::Mobile::ControlChoiceCount> ControlWords = {L"auto", L"mouse", L"touch"};

bool g_phoneView = false;
bool g_touchControls = false;

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

template <typename Choice, std::size_t Count>
std::wstring ToWord(Choice choice, const std::array<std::wstring_view, Count>& words)
{
    const auto index = static_cast<std::size_t>(choice);
    return std::wstring(index < Count ? words[index] : words[0]);
}

// A phone or tablet, as the browser page tells. A computer's touch screen does not count: its fingers
// already play the mouse, and its screen fits the PC layout.
bool IsHandheld()
{
#ifdef __EMSCRIPTEN__
    return mu::web::IsTouchDevice();
#else
    return false;
#endif
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

bool UI::Mobile::IsPhoneView()
{
    return g_phoneView;
}

bool UI::Mobile::IsTouchControls()
{
    return g_touchControls;
}

bool UI::Mobile::ReplacesPcHud()
{
    return g_phoneView && g_touchControls;
}

void UI::Mobile::Apply()
{
    const bool handheld = IsHandheld();
    const ViewChoice view = GetViewChoice();
    const ControlChoice controls = GetControlChoice();
    g_phoneView = view == ViewChoice::Phone || (view == ViewChoice::Auto && handheld);
    g_touchControls = controls == ControlChoice::Touch || (controls == ControlChoice::Auto && handheld);

    Core::Input::Touch::SetEnabled(g_touchControls);
    if (Rml::Context* context = RmlUiRuntime::Instance().GetContext())
    {
        context->ActivateTheme(MobileThemeFlag, g_phoneView);
        context->ActivateTheme(MobileHudThemeFlag, ReplacesPcHud());
    }
    // The flags move workspace regions (workspace.rcss): the windows on them follow next frame.
    UI::Placement::Invalidate();
}
