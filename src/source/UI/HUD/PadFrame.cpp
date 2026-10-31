#include "stdafx.h"
#include "UI/HUD/PadFrame.h"

#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadStick.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/HUD/PadTargets.h"

#include <RmlUi/Core/Context.h>
#include <SDL3/SDL.h>

#include <limits>

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
};
State g_state;

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
        Pad::Pointer::TurnWheel(window, -step.dy);
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
    if (dialogClosed && g_state.hasWindowPlace)
    {
        g_state.goBack = true;
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
        // A window just opened (or a hub tab turned) starts on its first cell or its first target.
        const bool opened = snapNow && !backNow && !lost;
        if (current == Pick::None || backNow || opened)
        {
            // After a dialog, back to the target nearest where the frame was before it.
            const Point from = g_state.goBack ? g_state.windowPlace : PointerPlace(scale);
            const int pick = opened ? Pick::First(targets) : Pick::Nearest(targets, from.x, from.y, Anywhere);
            if (pick != Pick::None)
                MoveOnto(window, targets[pick], scale);
            here = PointerPlace(scale);
            current = Pick::At(targets, here.x, here.y);
        }
        if (snapNow)
            g_state.snapPending = false;
        g_state.goBack = false;
    }
    const Pad::Step step = g_state.stepper.Take(presses, now);
    for (const Pad::Button button : {Pad::Button::DpadUp, Pad::Button::DpadDown, Pad::Button::DpadLeft,
                                     Pad::Button::DpadRight})
        presses[static_cast<int>(button)] = false;
    if (step.dx != 0 || step.dy != 0)
        Hop(window, targets, current, step, scale);
    FollowStick(window, targets, current, scale, *context);

    here = PointerPlace(scale);
    current = Pick::At(targets, here.x, here.y);
    g_state.onTarget = current != Pick::None;
    g_state.onItemPlace = g_state.onTarget && (targets[current].cell || targets[current].slot);
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
}

void Reset()
{
    g_state.snapPending = false;
    g_state.snapFrames = 0;
    g_state.placed = false;
    g_state.onTarget = false;
    g_state.onItemPlace = false;
    g_state.stickPushed = false;
    g_state.lostFrames = 0;
    g_state.onDialog = false;
    g_state.hasWindowPlace = false;
    g_state.goBack = false;
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
} // namespace UI::HUD::PadFrame
