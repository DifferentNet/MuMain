#include "stdafx.h"
#include "UI/HUD/PadFrame.h"

#include "Audio/DSPlaySound.h"
#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadStick.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/HUD/PadTargets.h"

#include <RmlUi/Core/Context.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <limits>
#include <map>
#include <string_view>
#include <vector>

namespace
{
namespace Pad = Core::Input::Gamepad;
namespace Pick = UI::HUD::PadTargetPick;
using UI::HUD::PadFrame::Clock;

// The frame hops at once with the d-pad or the stick, again after this while held, then at the second
// pace: the menu's.
constexpr auto FirstRepeat = std::chrono::milliseconds(300);
constexpr auto Repeat = std::chrono::milliseconds(100);
// The right stick let go snaps the pointer onto a target this near, as a share of the screen's height.
constexpr float SnapReachShare = 0.04f;
constexpr float Anywhere = std::numeric_limits<float>::max();
// The frame's waits go by the clock, so it lands the same at 30, 60 and 144 frames a second, but
// never under two frames: a window opened or a tab turned shows its targets a frame or two later.
constexpr int MinWaitFrames = 2;
// How long the frame waits before it snaps (a window opened), moves on from a target gone, or goes
// back after a dialog.
constexpr auto SnapDelay = std::chrono::milliseconds(30);
// A window that opens this long after the one the frame started in (the NPC shop after the
// inventory, its goods come from the server) takes the frame there, while it has not moved yet.
constexpr auto SettleTime = std::chrono::milliseconds(750);
// A hop ticks (the game's quiet button-move sound), but not quicker than this: a fast repeat stays calm.
constexpr auto TickRepeat = std::chrono::milliseconds(100);
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
    // Frames since the snap was asked for, and when: a window opened or a tab turned shows its targets
    // a frame or two later, the old window's may still be there before.
    int snapFrames = 0;
    Clock::time_point snapSince{};
    // The window whose page RT turned (SnapToPage): the snap starts on that page, -1 for the windows' first.
    int snapWindow = -1;
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
    bool onGauge = false;
    Pick::Box box;
    // The frame's target went away (a card opened over it, a dialog closed): it moves on a frame later,
    // with A up, so the release that closed a dialog does not land on the next target (the game reads
    // its windows' clicks at the pointer of the frame it sees the release in).
    int lostFrames = 0;
    Clock::time_point lostSince{};
    bool onDialog = false;
    // Where the frame rested in the windows before a dialog took it, to go back there after.
    bool hasWindowPlace = false;
    Point windowPlace;
    // Where goBack returns to (windowPlace as the dialog came up, or the dropdown).
    Point backPlace;
    bool goBack = false;
    int goBackFrames = 0;
    Clock::time_point goBackSince{};
    // A dialog that came up with its windows closed: no place there yet, so the frame starts on the
    // windows' first target, as a window opening, not on the one nearest the button pressed.
    bool goFirst = false;
    // A dropdown's list open: where the frame was on the dropdown, to go back there as the list closes
    // (a choice made or B), not stay on the row now under the option pressed.
    bool onPopup = false;
    Point popupPlace;
    // Frames since the frame started in a window just opened (and when), -1 once settled (it moved, or
    // time passed); the windows there were then.
    int settleFrames = -1;
    Clock::time_point settleSince{};
    int settleWindows = 0;
    // A menu item pressed (SnapOnNewWindow): the window it opens takes the frame on its first target;
    // the targets there before (knownTargets, by their boxes: native windows share one document) do not.
    bool newWindowPending = false;
    int newWindowFrames = 0;
    Clock::time_point newWindowSince{};
    // The new window's targets the frame last started on: a window fills in over a few frames.
    int newTargetCount = 0;
    std::vector<Pick::Box> knownTargets;
    // The targets' boxes in the last frame the frame played.
    std::vector<Pick::Box> lastTargets;
    // RB asked for the next window (NextWindow); where the frame last rested in each window, by its
    // `window`, to go back there; the windows and the frame's window as of the last Update.
    bool nextWindowPending = false;
    std::map<int, Point> windowPlaces;
    int windowCount = 0;
    int currentWindow = -1;
    // When the last hop ticked.
    Clock::time_point tickedAt{};
};
State g_state;
// Reused each frame, not built anew.
std::vector<int> g_windowScratch;
std::vector<Pick::Target> g_newTargets;
std::vector<int> g_pageIndexes;

// A wait of at least two frames and of `time`, begun `since`, `frames` frames counted.
bool Waited(int frames, Clock::time_point since, Clock::time_point now, Clock::duration time)
{
    return frames >= MinWaitFrames && now - since >= time;
}

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

// RB: the next window to the right of the one at `from` (by the windows' left edges, round to the
// leftmost), where the frame last rested there, else its first target.
void JumpToNextWindow(SDL_Window* window, std::span<const Pick::Target> targets, int from, float scale)
{
    std::map<int, float> lefts;
    for (const Pick::Target& target : targets)
    {
        const auto found = lefts.find(target.window);
        if (found == lefts.end())
            lefts.emplace(target.window, target.box.left);
        else
            found->second = std::min(found->second, target.box.left);
    }
    if (lefts.size() < 2)
        return;
    std::vector<std::pair<float, int>> order;
    for (const auto& [id, left] : lefts)
        order.emplace_back(left, id);
    std::sort(order.begin(), order.end());
    const int here = from == Pick::None ? order.back().second : targets[from].window;
    std::size_t at = 0;
    while (at < order.size() && order[at].second != here)
        ++at;
    const int next = order[(at + 1) % order.size()].second;
    std::vector<Pick::Target>& inNext = g_newTargets;
    inNext.clear();
    for (const Pick::Target& target : targets)
    {
        if (target.window == next)
            inNext.push_back(target);
    }
    int pick = Pick::None;
    if (const auto place = g_state.windowPlaces.find(next); place != g_state.windowPlaces.end())
        pick = Pick::Nearest(inNext, place->second.x, place->second.y, Anywhere);
    if (pick == Pick::None)
        pick = Pick::First(inNext);
    if (pick != Pick::None)
        MoveOnto(window, inNext[pick], scale);
}

// The page RT turned in `window`: its first target past the tabs (the tab just pressed is no place to
// start a page on), else its first; the windows' first with none there.
int FirstOnPage(std::span<const Pick::Target> targets, int window)
{
    for (const bool pastTabs : {true, false})
    {
        g_newTargets.clear();
        g_pageIndexes.clear();
        for (int i = 0; i < static_cast<int>(targets.size()); ++i)
        {
            if (targets[i].window == window && !(pastTabs && targets[i].page))
            {
                g_newTargets.push_back(targets[i]);
                g_pageIndexes.push_back(i);
            }
        }
        const int pick = Pick::First(g_newTargets);
        if (pick != Pick::None)
            return g_pageIndexes[pick];
    }
    return Pick::First(targets);
}

// The d-pad or the stick moves the frame that way; false when it stayed (no target that way).
bool Hop(SDL_Window* window, std::span<const Pick::Target> targets, int from, Pad::Step step, float scale)
{
    if (from == Pick::None)
    {
        const Point here = PointerPlace(scale);
        const int nearest = Pick::Nearest(targets, here.x, here.y, Anywhere);
        if (nearest == Pick::None)
            return false;
        MoveOnto(window, targets[nearest], scale);
        return true;
    }
    const int next = Pick::NextThatWay(targets, from, step.dx, step.dy);
    if (ScrollsInstead(targets, from, next, step.dy))
    {
        UI::HUD::PadTargets::ScrollList(targets[from].list, step.dy);
        return true;
    }
    if (next == Pick::None)
        return false;
    MoveOnto(window, targets[next], scale);
    return true;
}

void Tick(Clock::time_point now)
{
    if (now - g_state.tickedAt < TickRepeat)
        return;
    g_state.tickedAt = now;
    PlayBuffer(SOUND_MENU01);
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
    Update(window, presses, now, PadTargets::Collect());
}

void Update(SDL_Window* window, Pad::Presses& presses, Clock::time_point now, std::span<const Pick::Target> targets)
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (window == nullptr || context == nullptr)
        return;
    const float scale = ContextScale(window, *context);
    Point here = PointerPlace(scale);
    int current = Pick::At(targets, here.x, here.y);

    const bool onDialog = PadTargets::OnDialog();
    const bool dialogClosed = g_state.onDialog && !onDialog;
    g_state.onDialog = onDialog;
    const bool onPopup = PadTargets::OnPopup();
    if (onPopup && !g_state.onPopup)
        g_state.popupPlace = g_state.windowPlace;
    if (!onPopup && g_state.onPopup && !onDialog && g_state.hasWindowPlace)
    {
        g_state.backPlace = g_state.popupPlace;
        g_state.goBack = true;
        g_state.goFirst = false;
        g_state.goBackFrames = 0;
        g_state.goBackSince = now;
    }
    g_state.onPopup = onPopup;
    if (current == Pick::None && g_state.placed && (g_state.onTarget || g_state.lostFrames > 0))
    {
        if (g_state.lostFrames == 0)
            g_state.lostSince = now;
        ++g_state.lostFrames;
    }
    else
        g_state.lostFrames = 0;
    const bool buttonUp = !Pad::Pointer::IsButtonHeld(SDL_BUTTON_LEFT);
    const bool lost = Waited(g_state.lostFrames, g_state.lostSince, now, SnapDelay) && buttonUp;
    if (dialogClosed)
    {
        g_state.backPlace = g_state.windowPlace;
        g_state.goBack = true;
        g_state.goFirst = !g_state.hasWindowPlace;
        g_state.goBackFrames = 0;
        g_state.goBackSince = now;
    }
    if (g_state.goBack)
        ++g_state.goBackFrames;
    const bool backNow = g_state.goBack && Waited(g_state.goBackFrames, g_state.goBackSince, now, SnapDelay) && buttonUp;
    if (g_state.snapPending)
        ++g_state.snapFrames;
    const bool snapNow = g_state.snapPending && Waited(g_state.snapFrames, g_state.snapSince, now, SnapDelay);
    if ((snapNow || lost || backNow) && !targets.empty())
    {
        g_state.lostFrames = 0;
        // A window just opened (or a hub tab turned) starts on its first cell or its first target, also
        // when the last window's target went away in the same frame (a window opened from the menu).
        const bool opened = snapNow && !backNow;
        if (current == Pick::None || backNow || opened)
        {
            // After a dialog, back to the target nearest where the frame was before it. A page RT turned
            // starts on that page, wherever the frame was.
            const Point from = g_state.goBack ? g_state.backPlace : PointerPlace(scale);
            int pick = Pick::None;
            if (opened && g_state.snapWindow >= 0)
                pick = FirstOnPage(targets, g_state.snapWindow);
            else if (opened || (backNow && g_state.goFirst))
                pick = Pick::First(targets);
            else
                pick = Pick::Nearest(targets, from.x, from.y, Anywhere);
            if (pick != Pick::None)
                MoveOnto(window, targets[pick], scale);
            here = PointerPlace(scale);
            current = Pick::At(targets, here.x, here.y);
        }
        if (snapNow)
        {
            g_state.snapPending = false;
            g_state.snapWindow = -1;
        }
        g_state.settleFrames = opened ? 0 : -1;
        g_state.settleSince = now;
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
            g_state.settleSince = now;
            g_state.settleWindows = CountWindows(targets);
        }
        if (Waited(g_state.newWindowFrames, g_state.newWindowSince, now, SettleTime))
            g_state.newWindowPending = false;
    }
    g_state.lastTargets.clear();
    for (const Pick::Target& target : targets)
        g_state.lastTargets.push_back(target.box);
    const Pad::Step step = g_state.stepper.Take(presses, now);
    for (const Pad::Button button : {Pad::Button::DpadUp, Pad::Button::DpadDown, Pad::Button::DpadLeft,
                                     Pad::Button::DpadRight})
        presses[static_cast<int>(button)] = false;
    // On a slider the d-pad's sides change it; up and down leave it.
    if (step.dx != 0 && step.dy == 0 && current != Pick::None && targets[current].gauge)
        PadTargets::StepGauge(current, step.dx);
    else if ((step.dx != 0 || step.dy != 0) && Hop(window, targets, current, step, scale))
        Tick(now);
    if (g_state.nextWindowPending)
    {
        g_state.nextWindowPending = false;
        if (!onDialog)
        {
            JumpToNextWindow(window, targets, current, scale);
            g_state.newWindowPending = false;
            g_state.settleFrames = -1;
        }
    }
    if (step.dx != 0 || step.dy != 0 || g_state.stickPushed)
        g_state.newWindowPending = false;
    if (g_state.settleFrames >= 0)
    {
        ++g_state.settleFrames;
        const bool settled = Waited(g_state.settleFrames, g_state.settleSince, now, SettleTime);
        if (step.dx != 0 || step.dy != 0 || g_state.stickPushed || settled)
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
    g_state.onGauge = g_state.onTarget && g_state.placed && targets[current].gauge;
    if (g_state.onTarget)
        g_state.box = targets[current].box;
    if (g_state.onTarget && !onDialog)
    {
        g_state.windowPlace = here;
        g_state.hasWindowPlace = true;
        g_state.windowPlaces[targets[current].window] = here;
    }
    g_state.windowCount = onDialog ? 0 : CountWindows(targets);
    g_state.currentWindow = g_state.onTarget && !onDialog ? targets[current].window : -1;
}

void SnapSoon()
{
    g_state.snapPending = true;
    g_state.snapFrames = 0;
    g_state.snapSince = Clock::now();
    g_state.snapWindow = -1;
    g_state.settleFrames = -1;
}

void SnapToPage(int window)
{
    SnapSoon();
    g_state.snapWindow = window;
}

void SnapOnNewWindow()
{
    g_state.knownTargets = g_state.lastTargets;
    g_state.newWindowPending = true;
    g_state.newWindowFrames = 0;
    g_state.newWindowSince = Clock::now();
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
    g_state.onGauge = false;
    g_state.stickPushed = false;
    g_state.lostFrames = 0;
    g_state.onDialog = false;
    g_state.onPopup = false;
    g_state.hasWindowPlace = false;
    g_state.goBack = false;
    g_state.goFirst = false;
    g_state.stepper.Reset();
    g_state.nextWindowPending = false;
    g_state.windowPlaces.clear();
    g_state.windowCount = 0;
    g_state.currentWindow = -1;
}

void NextWindow()
{
    g_state.nextWindowPending = true;
}

int WindowCount()
{
    return g_state.windowCount;
}

int CurrentWindow()
{
    return g_state.currentWindow;
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

bool OnGauge()
{
    return g_state.onGauge;
}
} // namespace UI::HUD::PadFrame
