#include "stdafx.h"
#include "UI/HUD/PadFrame.h"

#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadStick.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/HUD/PadTargets.h"

#include <RmlUi/Core/Context.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <limits>
#include <string_view>
#include <vector>

namespace
{
namespace Pad = Core::Input::Gamepad;
namespace Pick = UI::HUD::PadTargetPick;
using UI::HUD::PadFrame::Clock;

// The frame hops at once with the d-pad or the stick, again after this while held, then at the second
// pace: a little slower than the menu's, as a window's cells are small.
constexpr auto FirstRepeat = std::chrono::milliseconds(380);
constexpr auto Repeat = std::chrono::milliseconds(130);
// The right stick let go snaps the pointer onto a target this near, as a share of the screen's height.
constexpr float SnapReachShare = 0.04f;
constexpr float Anywhere = std::numeric_limits<float>::max();
constexpr int SnapDelayFrames = 2;
// A window that opens this many frames after the one the frame started in (the NPC shop after the
// inventory, its goods come from the server) takes the frame there, while it has not moved yet.
constexpr int SettleFrames = 45;
// The NPC shop's window: its cells are goods, A buys them.
constexpr std::string_view ShopDocument = "npc_shop.rml";
// The crafting box: its items have no card.
constexpr std::string_view MixDocument = "mix_inventory.rml";

struct Point
{
    float x = 0.f;
    float y = 0.f;
};

struct State
{
    Pad::Stepper stepper{FirstRepeat, Repeat};
    bool snapPending = false;
    // Frames since the snap was asked for: a window opened or a tab turned shows its targets a frame
    // or two later, the old window's may still be there before.
    int snapFrames = 0;
    // The right stick moved the pointer last frame.
    bool stickPushed = false;
    // Where the frame put the pointer, in context pixels: the pointer's own place catches up a frame
    // later (its moves are queued events).
    bool placed = false;
    Point place;
    bool onTarget = false;
    bool onItemPlace = false;
    bool onWornPlace = false;
    bool onGoods = false;
    bool onMixItem = false;
    bool onEmptyCell = false;
    Pick::Box box;
    // The frame's target went away (a card opened over it, a dialog closed): it moves on a frame later,
    // with A up, so the release that closed a dialog does not land on the next target (the game reads
    // its windows' clicks at the pointer of the frame it sees the release in).
    int lostFrames = 0;
    bool onDialog = false;
    // Where the frame rested in the windows before a dialog took it, to go back there after.
    bool hasWindowPlace = false;
    Point windowPlace;
    bool goBack = false;
    int goBackFrames = 0;
    // A dialog that came up with its windows closed: no place there yet, so the frame starts on the
    // windows' first target, as a window opening, not on the one nearest the button pressed.
    bool goFirst = false;
    // Frames since the frame started in a window just opened, -1 once settled (it moved, or time
    // passed); the windows there were then.
    int settleFrames = -1;
    int settleWindows = 0;
    // A menu item pressed (SnapOnNewWindow): the window it opens takes the frame on its first target;
    // the targets there before (knownTargets, by their boxes: native windows share one document) do not.
    bool newWindowPending = false;
    int newWindowFrames = 0;
    // The new window's targets the frame last started on: a window fills in over a few frames.
    int newTargetCount = 0;
    std::vector<Pick::Box> knownTargets;
    // The targets' boxes in the last frame the frame played.
    std::vector<Pick::Box> lastTargets;
};
State g_state;
// Reused each frame, not built anew.
std::vector<int> g_windowScratch;
std::vector<Pick::Target> g_newTargets;

// Context pixels per window point: the context is the drawable's size, the pointer the window's.
float ContextScale(SDL_Window* window, const Rml::Context& context)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    return width > 0 ? static_cast<float>(context.GetDimensions().x) / static_cast<float>(width) : 1.f;
}

Point PointerPlace(float scale)
{
    if (g_state.placed)
        return g_state.place;
    float x = 0.f;
    float y = 0.f;
    SDL_GetMouseState(&x, &y);
    return {x * scale, y * scale};
}

// How many windows the targets are in.
int CountWindows(std::span<const Pick::Target> targets)
{
    std::vector<int>& windows = g_windowScratch;
    windows.clear();
    for (const Pick::Target& target : targets)
        if (std::find(windows.begin(), windows.end(), target.window) == windows.end())
            windows.push_back(target.window);
    return static_cast<int>(windows.size());
}

bool SameBox(const Pick::Box& a, const Pick::Box& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

void MoveOnto(SDL_Window* window, const Pick::Target& target, float scale)
{
    g_state.place = {target.box.CentreX(), target.box.CentreY()};
    g_state.placed = true;
    Pad::Pointer::MoveTo(window, g_state.place.x / scale, g_state.place.y / scale);
}

// Whether a step up or down should scroll the list the frame is in: the list goes on that way and the
// next target that way is not in it.
bool ScrollsInstead(std::span<const Pick::Target> targets, int from, int next, int dy)
{
    const Pick::Target& here = targets[from];
    if (dy == 0 || here.list == Pick::None)
        return false;
    const bool goesOn = dy < 0 ? here.listScrollsUp : here.listScrollsDown;
    return goesOn && (next == Pick::None || targets[next].list != here.list);
}

void Hop(SDL_Window* window, std::span<const Pick::Target> targets, int from, Pad::Step step, float scale)
{
    if (from == Pick::None)
    {
        const Point here = PointerPlace(scale);
        const int nearest = Pick::Nearest(targets, here.x, here.y, Anywhere);
        if (nearest != Pick::None)
            MoveOnto(window, targets[nearest], scale);
        return;
    }
    const int next = Pick::NextThatWay(targets, from, step.dx, step.dy);
    if (ScrollsInstead(targets, from, next, step.dy))
        UI::HUD::PadTargets::ScrollList(targets[from].list, step.dy);
    else if (next != Pick::None)
        MoveOnto(window, targets[next], scale);
}

// The right stick: while it moves the pointer the frame follows it; let go off every target, the
// pointer snaps onto one near.
void FollowStick(SDL_Window* window, std::span<const Pick::Target> targets, int current, float scale,
                 const Rml::Context& context)
{
    const bool pushed = Pad::IsPushed(Pad::RightStick(), g_state.stickPushed);
    if (pushed)
        g_state.placed = false;
    const bool letGo = g_state.stickPushed && !pushed;
    g_state.stickPushed = pushed;
    if (!letGo || current != Pick::None)
        return;
    const Point here = PointerPlace(scale);
    const float reach = static_cast<float>(context.GetDimensions().y) * SnapReachShare;
    const int nearest = Pick::Nearest(targets, here.x, here.y, reach);
    if (nearest != Pick::None)
        MoveOnto(window, targets[nearest], scale);
}
} // namespace

namespace UI::HUD::PadFrame
{
void Update(SDL_Window* window, Pad::Presses& presses, Clock::time_point now)
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (window == nullptr || context == nullptr)
        return;
    const std::span<const Pick::Target> targets = PadTargets::Collect();
    const float scale = ContextScale(window, *context);
    Point here = PointerPlace(scale);
    int current = Pick::At(targets, here.x, here.y);

    const bool onDialog = PadTargets::OnDialog();
    const bool dialogClosed = g_state.onDialog && !onDialog;
    g_state.onDialog = onDialog;
    if (current == Pick::None && g_state.placed && (g_state.onTarget || g_state.lostFrames > 0))
        ++g_state.lostFrames;
    else
        g_state.lostFrames = 0;
    const bool buttonUp = !Pad::Pointer::IsButtonHeld(SDL_BUTTON_LEFT);
    const bool lost = g_state.lostFrames >= 2 && buttonUp;
    if (dialogClosed)
    {
        g_state.goBack = true;
        g_state.goFirst = !g_state.hasWindowPlace;
        g_state.goBackFrames = 0;
    }
    if (g_state.goBack)
        ++g_state.goBackFrames;
    const bool backNow = g_state.goBack && g_state.goBackFrames >= 2 && buttonUp;
    if (g_state.snapPending)
        ++g_state.snapFrames;
    const bool snapNow = g_state.snapPending && g_state.snapFrames >= SnapDelayFrames;
    if ((snapNow || lost || backNow) && !targets.empty())
    {
        g_state.lostFrames = 0;
        // A window just opened (or a hub tab turned) starts on its first cell or its first target, also
        // when the last window's target went away in the same frame (a window opened from the menu).
        const bool opened = snapNow && !backNow;
        if (current == Pick::None || backNow || opened)
        {
            // After a dialog, back to the target nearest where the frame was before it.
            const Point from = g_state.goBack ? g_state.windowPlace : PointerPlace(scale);
            const int pick = opened || (backNow && g_state.goFirst) ? Pick::First(targets)
                                                                    : Pick::Nearest(targets, from.x, from.y, Anywhere);
            if (pick != Pick::None)
                MoveOnto(window, targets[pick], scale);
            here = PointerPlace(scale);
            current = Pick::At(targets, here.x, here.y);
        }
        if (snapNow)
            g_state.snapPending = false;
        g_state.settleFrames = opened ? 0 : -1;
        g_state.settleWindows = CountWindows(targets);
        g_state.goBack = false;
        g_state.goFirst = false;
    }
    if (g_state.newWindowPending)
    {
        // The window a menu item opened: the frame starts on its first target, again as more of it shows
        // (a list fills in a frame later), until the frame is moved; the windows already open keep
        // theirs. Nothing new in a while (the item opened nothing): the frame stays.
        ++g_state.newWindowFrames;
        g_newTargets.clear();
        for (const Pick::Target& target : targets)
        {
            const auto known = std::find_if(g_state.knownTargets.begin(), g_state.knownTargets.end(),
                                            [&target](const Pick::Box& box) { return SameBox(box, target.box); });
            if (known == g_state.knownTargets.end())
                g_newTargets.push_back(target);
        }
        const int pick = Pick::First(g_newTargets);
        if (pick != Pick::None && static_cast<int>(g_newTargets.size()) > g_state.newTargetCount)
        {
            MoveOnto(window, g_newTargets[pick], scale);
            here = PointerPlace(scale);
            current = Pick::At(targets, here.x, here.y);
            g_state.newTargetCount = static_cast<int>(g_newTargets.size());
            g_state.snapPending = false;
            g_state.lostFrames = 0;
            g_state.settleFrames = 0;
            g_state.settleWindows = CountWindows(targets);
        }
        if (g_state.newWindowFrames > SettleFrames)
            g_state.newWindowPending = false;
    }
    g_state.lastTargets.clear();
    for (const Pick::Target& target : targets)
        g_state.lastTargets.push_back(target.box);
    const Pad::Step step = g_state.stepper.Take(presses, now);
    for (const Pad::Button button : {Pad::Button::DpadUp, Pad::Button::DpadDown, Pad::Button::DpadLeft,
                                     Pad::Button::DpadRight})
        presses[static_cast<int>(button)] = false;
    if (step.dx != 0 || step.dy != 0)
        Hop(window, targets, current, step, scale);
    if (step.dx != 0 || step.dy != 0 || g_state.stickPushed)
        g_state.newWindowPending = false;
    if (g_state.settleFrames >= 0)
    {
        ++g_state.settleFrames;
        if (step.dx != 0 || step.dy != 0 || g_state.stickPushed || g_state.settleFrames > SettleFrames)
            g_state.settleFrames = -1;
        else if (CountWindows(targets) > g_state.settleWindows)
            SnapSoon();
    }
    FollowStick(window, targets, current, scale, *context);

    here = PointerPlace(scale);
    current = Pick::At(targets, here.x, here.y);
    g_state.onTarget = current != Pick::None;
    g_state.onItemPlace = g_state.onTarget && (targets[current].cell || targets[current].slot);
    g_state.onWornPlace = g_state.onTarget && targets[current].slot;
    g_state.onGoods = g_state.onItemPlace && UI::HUD::PadTargets::IsIn(targets[current], ShopDocument);
    g_state.onMixItem = g_state.onItemPlace && UI::HUD::PadTargets::IsIn(targets[current], MixDocument);
    g_state.onEmptyCell = g_state.onTarget && targets[current].empty;
    if (g_state.onTarget)
        g_state.box = targets[current].box;
    if (g_state.onTarget && !onDialog)
    {
        g_state.windowPlace = here;
        g_state.hasWindowPlace = true;
    }
}

void SnapSoon()
{
    g_state.snapPending = true;
    g_state.snapFrames = 0;
    g_state.settleFrames = -1;
}

void SnapOnNewWindow()
{
    g_state.knownTargets = g_state.lastTargets;
    g_state.newWindowPending = true;
    g_state.newWindowFrames = 0;
    g_state.newTargetCount = 0;
}

void Reset()
{
    g_state.newWindowPending = false;
    g_state.lastTargets.clear();
    g_state.snapPending = false;
    g_state.settleFrames = -1;
    g_state.snapFrames = 0;
    g_state.placed = false;
    g_state.onTarget = false;
    g_state.onItemPlace = false;
    g_state.onWornPlace = false;
    g_state.onGoods = false;
    g_state.onMixItem = false;
    g_state.onEmptyCell = false;
    g_state.stickPushed = false;
    g_state.lostFrames = 0;
    g_state.onDialog = false;
    g_state.hasWindowPlace = false;
    g_state.goBack = false;
    g_state.goFirst = false;
    g_state.stepper.Reset();
}

bool Box(PadTargetPick::Box& box)
{
    if (!g_state.onTarget)
        return false;
    box = g_state.box;
    return true;
}

bool OnItemPlace()
{
    return g_state.onItemPlace;
}

bool OnWornPlace()
{
    return g_state.onWornPlace;
}

bool OnGoods()
{
    return g_state.onGoods;
}

bool OnMixItem()
{
    return g_state.onMixItem;
}

bool OnEmptyCell()
{
    return g_state.onEmptyCell;
}
} // namespace UI::HUD::PadFrame
