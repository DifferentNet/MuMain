#include "stdafx.h"
#include "UI/Core/WindowWarmUp.h"

#ifdef __EMSCRIPTEN__
#include "Core/Utilities/FirstOpenTiming.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlInvisibleShow.h"
#include "World/MapInfra/MapManager.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iterator>

namespace UI::WarmUp
{
namespace
{
using mu::ui::window::CObject;

struct WarmUpWindow
{
    const char* name;
    DWORD id;
};

// The windows of the hotkeys and the phone's top row and menu that have a document of their own, the
// most opened first. Not here: the skill picker and the phone's menu and Social panel (parts of the
// touch controls' document, always shown), the skill grid (part of the main frame's), the chat line
// (small, and it holds the text field the phone's keyboard follows), Friends (its document belongs to the
// legacy friend window manager, CUIFriendWindow, with no document to show on its own).
constexpr WarmUpWindow kWarmUpWindows[] = {
    {"Inventory", mu::ui::window::INTERFACE_INVENTORY},
    {"Character", mu::ui::window::INTERFACE_CHARACTER},
    {"MiniMap", mu::ui::window::INTERFACE_MINI_MAP},
    {"Options", mu::ui::window::INTERFACE_OPTION},
    {"Teleport", mu::ui::window::INTERFACE_MOVEMAP},
    {"Quest", mu::ui::window::INTERFACE_MYQUEST},
    {"Party", mu::ui::window::INTERFACE_PARTY},
    {"Guild", mu::ui::window::INTERFACE_GUILDINFO},
    {"Daily", mu::ui::window::INTERFACE_DAILY_REWARDS},
    {"Events", mu::ui::window::INTERFACE_EVENT_SCHEDULE},
    {"BattlePass", mu::ui::window::INTERFACE_BATTLE_PASS},
};
constexpr int kWarmUpWindowCount = static_cast<int>(std::size(kWarmUpWindows));
constexpr int kNoWindow = -1;
constexpr int kNoWorld = -1;

// After entering the world or another map: the map's own loads come first.
constexpr double kStartDelayMs = 5000.0;
// A heavier frame waits: the warm-up adds its window's first layout to the frame.
constexpr double kLightFrameMs = 40.0;
// A device whose frames never get that light: after this long a frame up to kSlowDeviceFrameMs will do.
constexpr double kSlowDeviceWaitMs = 30000.0;
constexpr double kSlowDeviceFrameMs = 80.0;

struct State
{
    // Per session.
    bool warmed[kWarmUpWindowCount] = {};
    // Had nothing to show (opened by the player, no document, no map picture): tried again in the next
    // world, so it holds up no other window.
    bool skipped[kWarmUpWindowCount] = {};

    bool inWorld = false;
    int world = kNoWorld;
    double quietSince = 0.0;
    // Since when a window waits only for a lighter frame; 0 while none does.
    double heavySince = 0.0;

    // Picked by Tick() for this frame, then shown between Context::Update() and Context::Render().
    int picked = kNoWindow;
    int shown = kNoWindow;
    double shownAt = 0.0;
    UI::RmlBridge::InvisibleShow show;

    // Logged at the next Tick(), with its frame's time.
    int logged = kNoWindow;
    double loggedMs = 0.0;
    int warmedInPass = 0;
};

State& Warm()
{
    static State state;
    return state;
}

int NextWindow(const State& state)
{
    for (int i = 0; i < kWarmUpWindowCount; ++i)
    {
        if (!state.warmed[i] && !state.skipped[i])
            return i;
    }
    return kNoWindow;
}

void LogWarmed(State& state, double frameMs)
{
    if (state.logged == kNoWindow)
        return;
    mu::log::Get("render")->info("Warm-up: {} in {:.1f} ms (frame {:.1f} ms)", kWarmUpWindows[state.logged].name,
                                 state.loggedMs, frameMs);
    state.logged = kNoWindow;
}

void LogDone(State& state)
{
    if (state.warmedInPass == 0)
        return;
    mu::log::Get("render")->info("Warm-up: done, {} windows", state.warmedInPass);
    state.warmedInPass = 0;
}

// Restarts the wait on entering the world and on each new map.
bool FollowWorld(State& state, double now)
{
    const bool inWorld = SceneFlag == MAIN_SCENE && Hero != nullptr;
    if (!inWorld)
    {
        state.inWorld = false;
        return false;
    }
    if (!state.inWorld || state.world != gMapManager.WorldActive)
    {
        state.inWorld = true;
        state.world = gMapManager.WorldActive;
        state.quietSince = now;
        state.heavySince = 0.0;
        std::fill(std::begin(state.skipped), std::end(state.skipped), false);
    }
    return true;
}

// A mouse button held or a finger on the screen: the player is busy with the hover and the press chain.
bool PointerDown()
{
    if (SDL_GetMouseState(nullptr, nullptr) != 0)
        return true;
    int devices = 0;
    SDL_TouchID* touchDevices = SDL_GetTouchDevices(&devices);
    bool down = false;
    for (int i = 0; touchDevices != nullptr && i < devices && !down; ++i)
    {
        int fingers = 0;
        if (SDL_Finger** list = SDL_GetTouchFingers(touchDevices[i], &fingers))
        {
            down = fingers > 0;
            SDL_free(list);
        }
    }
    SDL_free(touchDevices);
    return down;
}

bool FrameLightEnough(State& state, double lastFrameMs, double now)
{
    if (lastFrameMs < kLightFrameMs)
    {
        state.heavySince = 0.0;
        return true;
    }
    if (state.heavySince == 0.0)
        state.heavySince = now;
    return now - state.heavySince >= kSlowDeviceWaitMs && lastFrameMs < kSlowDeviceFrameMs;
}

// The window's document, filled in as its first opening would; null when there is nothing to warm.
Rml::ElementDocument* PreparedDocument(int index)
{
    CObject* window = g_pNewUIMng->FindUIObj(kWarmUpWindows[index].id);
    if (window == nullptr || g_pNewUISystem->IsVisible(kWarmUpWindows[index].id))
        return nullptr;
    if (!window->PrepareWarmUp())
        return nullptr;
    return window->GetWarmUpDocument();
}
} // namespace

void Tick(double lastFrameMs)
{
    State& state = Warm();
    LogWarmed(state, lastFrameMs);
    // Begin and End share one RmlUi frame; a frame that never reached End() puts the document back here.
    state.show.End();
    state.shown = kNoWindow;
    state.picked = kNoWindow;

    if (!GameConfig::GetInstance().GetWarmUpWindows())
        return;
    const double now = Core::FirstOpenTiming::NowMs();
    if (!FollowWorld(state, now) || now - state.quietSince < kStartDelayMs)
        return;
    const int next = NextWindow(state);
    if (next == kNoWindow)
    {
        LogDone(state);
        return;
    }
    if (!FrameLightEnough(state, lastFrameMs, now) || PointerDown())
        return;
    state.picked = next;
}

void BeforeRmlUpdate()
{
    State& state = Warm();
    const int index = state.picked;
    if (index == kNoWindow)
        return;
    state.picked = kNoWindow;

    if (!state.show.Begin(PreparedDocument(index)))
    {
        state.skipped[index] = true;
        return;
    }
    state.warmed[index] = true;
    state.shown = index;
    state.shownAt = Core::FirstOpenTiming::NowMs();
}

bool AfterRmlRender()
{
    State& state = Warm();
    if (state.shown == kNoWindow)
        return false;
    state.show.End();
    state.logged = state.shown;
    state.loggedMs = Core::FirstOpenTiming::NowMs() - state.shownAt;
    state.shown = kNoWindow;
    ++state.warmedInPass;
    state.heavySince = 0.0;
    return true;
}
} // namespace UI::WarmUp
#endif
