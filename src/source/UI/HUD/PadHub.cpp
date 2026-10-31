#include "stdafx.h"
#include "UI/HUD/PadHub.h"

#include "I18N/All.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/TouchControls.h"

#include <array>

namespace
{
using UI::HUD::PadHub::Clock;
namespace Controls = UI::HUD::TouchControls;

// A tab's window opens a frame or two after it is asked to: until then the hub does not take it for
// closed.
constexpr auto OpeningGrace = std::chrono::milliseconds(300);
// The skill picker's tab has no window of its own.
constexpr int SkillPicker = -1;

struct Tab
{
    const wchar_t* const* label;
    int window;
};

const std::array<Tab, 6> Tabs = {{
    {&I18N::Game::TouchMenuInventory, mu::ui::window::INTERFACE_INVENTORY},
    {&I18N::Game::TouchMenuCharacter, mu::ui::window::INTERFACE_CHARACTER},
    {&I18N::Game::TouchMenuSkills, SkillPicker},
    {&I18N::Game::TouchMenuQuests, mu::ui::window::INTERFACE_MYQUEST},
    {&I18N::Game::TouchMenuGuild, mu::ui::window::INTERFACE_GUILDINFO},
    {&I18N::Game::PadHubSettings, mu::ui::window::INTERFACE_OPTION},
}};
constexpr int TabTotal = static_cast<int>(Tabs.size());

struct State
{
    bool open = false;
    int tab = 0;
    Clock::time_point shownAt{};
};
State g_state;

bool IsShown(int tab)
{
    const int window = Tabs[tab].window;
    return window == SkillPicker ? Controls::IsMenuOpen() : g_pNewUISystem->IsVisible(window);
}

// Opens a tab's window: false where it cannot open (no skill picker without the on-screen controls).
bool Show(int tab)
{
    const int window = Tabs[tab].window;
    if (window == SkillPicker)
        return Controls::OpenSkillPicker();
    if (!g_pNewUISystem->IsVisible(window))
        g_pNewUISystem->Show(window);
    return true;
}

void Hide(int tab)
{
    const int window = Tabs[tab].window;
    if (window == SkillPicker)
    {
        if (Controls::IsMenuOpen())
            Controls::CloseMenu();
        return;
    }
    if (g_pNewUISystem->IsVisible(window))
        g_pNewUISystem->Hide(window);
}

// Shows the tab, or the next one that way that opens.
void ShowFrom(int tab, int step, Clock::time_point now)
{
    for (int tried = 0; tried < TabTotal; ++tried)
    {
        const int next = ((tab + step * tried) % TabTotal + TabTotal) % TabTotal;
        if (Show(next))
        {
            g_state.tab = next;
            g_state.shownAt = now;
            return;
        }
    }
    g_state.open = false;
}
} // namespace

namespace UI::HUD::PadHub
{
bool IsOpen()
{
    return g_state.open;
}

void Open(Clock::time_point now)
{
    g_state.open = true;
    ShowFrom(g_state.tab, 1, now);
}

void Close()
{
    if (!g_state.open)
        return;
    g_state.open = false;
    Hide(g_state.tab);
}

void Turn(int step, Clock::time_point now)
{
    if (!g_state.open)
        return;
    Hide(g_state.tab);
    ShowFrom(g_state.tab + step, step, now);
}

void Update(Clock::time_point now)
{
    if (g_state.open && now - g_state.shownAt >= OpeningGrace && !IsShown(g_state.tab))
        g_state.open = false;
}

int TabCount()
{
    return TabTotal;
}

int CurrentTab()
{
    return g_state.tab;
}

const wchar_t* TabLabel(int tab)
{
    return tab >= 0 && tab < TabTotal ? *Tabs[tab].label : L"";
}

bool IsOnSkills()
{
    return g_state.open && Tabs[g_state.tab].window == SkillPicker;
}
} // namespace UI::HUD::PadHub
