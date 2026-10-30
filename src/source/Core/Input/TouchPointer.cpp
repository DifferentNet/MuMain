#include "stdafx.h"
#include "Core/Input/TouchPointer.h"

#include "Core/Input/TouchGesture.h"
#include "Core/Input/UiInputRouter.h"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace Core::Input::Touch::Pointer
{
namespace
{
using Clock = std::chrono::steady_clock;
using Gesture::Phase;

// A second tap this soon after the first, close to it, is a double click.
constexpr auto DoubleTapTime = std::chrono::milliseconds(350);

// Distances are fractions of the window's height: a finger is about the same share of every phone's
// screen, whatever its pixels.
// How far a tapping finger may wander before the tap turns into a drag.
constexpr float TapSlop = 0.03f;
// How close the second tap of a double click must land to the first.
constexpr float DoubleTapSlop = 0.06f;
// How much the pinching fingers' distance changes for one step of the mouse wheel.
constexpr float PinchStep = 0.06f;
// A scrolling finger's speed is smoothed over about this long (seconds): the last moves before it lifts.
constexpr float VelocitySmoothingSeconds = 0.05f;
// A scrolling finger held still this long before it lifts flicks nothing.
constexpr auto FlickHoldTime = std::chrono::milliseconds(80);
// Above and left of the window, in window pixels: its UI hovers nothing there.
constexpr float OffWindow = -1.f;

constexpr int ButtonSlots = 4; // SDL_BUTTON_LEFT, _MIDDLE and _RIGHT index it

struct State
{
    Phase phase = Phase::None;
    SDL_FingerID first = 0;
    SDL_FingerID second = 0;
    bool firstDown = false;
    bool secondDown = false;
    float downX = 0.f;
    float downY = 0.f;
    float x = 0.f;
    float y = 0.f;
    float secondX = 0.f;
    float secondY = 0.f;
    // The fingers' distance at the last wheel step.
    float pinchDistance = 0.f;
    Clock::time_point downTime;
    // A scrolling finger's last move and its smoothed speed, window pixels a second.
    Clock::time_point moveTime;
    float velocityX = 0.f;
    float velocityY = 0.f;
    // The finger stopped a gliding list as it landed: lifting it clicks nothing.
    bool stoppedGlide = false;
    // A drag has just pressed the left button where the finger landed. The pointer stays there
    // until the next Tick, so the game's frame sees the press at that point, then follows the finger.
    bool heldAtLanding = false;
    Clock::time_point lastTapTime;
    float lastTapX = 0.f;
    float lastTapY = 0.f;
    std::array<bool, ButtonSlots> buttons{};
};

State g_state;
// The pointer is where a finger left it (and not taken off the window since, nor pressed with by the
// mouse or the pad).
bool g_fingerPlaced = false;
// Outside State: a tap's press and release reach the game's windows after the finger has lifted.
LeftPress g_lastLeftPress = LeftPress::NotFinger;

float WindowHeight(SDL_Window* window)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    return static_cast<float>(height > 0 ? height : 1);
}

float Distance(float ax, float ay, float bx, float by)
{
    return std::hypot(ax - bx, ay - by);
}

// The mouse moves as a real one does: SDL's own pointer follows (what GetCursorPos reads) and the
// motion reaches the window's UI and the game.
void MovePointer(SDL_Window* window, float x, float y)
{
    SDL_WarpMouseInWindow(window, x, y);
}

// A motion only the window's UI and the game see. SDL keeps a warped pointer inside the window,
// so a pointer taken off it is told this way.
void PushMotion(SDL_Window* window, float x, float y)
{
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp = SDL_GetTicksNS();
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.which = SDL_TOUCH_MOUSEID;
    event.motion.x = x;
    event.motion.y = y;
    SDL_PushEvent(&event);
}

// Takes the pointer off the window: SDL's own pointer goes to the corner, the UI and the game see
// it outside, where nothing shows a hover or takes a click.
void LeaveWindow(SDL_Window* window)
{
    MovePointer(window, OffWindow, OffWindow);
    PushMotion(window, OffWindow, OffWindow);
}

// The button event goes at (x, y), where the UI and the game take the press or the release.
void PushButton(SDL_Window* window, Uint8 button, bool down, Uint8 clicks, float x, float y)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.timestamp = SDL_GetTicksNS();
    event.button.windowID = SDL_GetWindowID(window);
    event.button.which = SDL_TOUCH_MOUSEID;
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = clicks;
    event.button.x = x;
    event.button.y = y;
    SDL_PushEvent(&event);
    if (button < ButtonSlots)
        g_state.buttons[button] = down;
}

void PushWheel(SDL_Window* window, int steps)
{
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = SDL_GetTicksNS();
    event.wheel.windowID = SDL_GetWindowID(window);
    event.wheel.which = SDL_TOUCH_MOUSEID;
    event.wheel.y = static_cast<float>(steps);
    event.wheel.integer_y = steps;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x = g_state.x;
    event.wheel.mouse_y = g_state.y;
    SDL_PushEvent(&event);
}

bool HoldsAnyButton()
{
    for (const bool held : g_state.buttons)
    {
        if (held)
            return true;
    }
    return false;
}

// Lets go of the buttons the finger holds off the window: a gesture broken off (the page hidden, the
// screen turned) drops and clicks nothing.
void ReleaseButtons(SDL_Window* window)
{
    if (!HoldsAnyButton())
        return;
    LeaveWindow(window);
    for (Uint8 button = 0; button < ButtonSlots; ++button)
    {
        if (g_state.buttons[button])
            PushButton(window, button, false, 1, OffWindow, OffWindow);
    }
}

// A drag called off (the system took the finger, or a second finger turned it into a pinch): the
// left button is let go off the window, so nothing under the finger takes it as a drop or a click.
void CancelDrag(SDL_Window* window)
{
    g_state.heldAtLanding = false;
    LeaveWindow(window);
    PushButton(window, SDL_BUTTON_LEFT, false, 1, OffWindow, OffWindow);
}

Uint8 TapClicks(float x, float y, float height)
{
    const auto now = Clock::now();
    const bool soon = now - g_state.lastTapTime <= DoubleTapTime;
    const bool close = Distance(x, y, g_state.lastTapX, g_state.lastTapY) <= DoubleTapSlop * height;
    g_state.lastTapTime = now;
    g_state.lastTapX = x;
    g_state.lastTapY = y;
    return soon && close ? 2 : 1;
}

void Tap(SDL_Window* window)
{
    const Uint8 clicks = TapClicks(g_state.x, g_state.y, WindowHeight(window));
    g_lastLeftPress = LeftPress::Tap;
    PushButton(window, SDL_BUTTON_LEFT, true, clicks, g_state.x, g_state.y);
    PushButton(window, SDL_BUTTON_LEFT, false, clicks, g_state.x, g_state.y);
}

// Smooths the scrolling finger's speed from its move by (dx, dy) since the last.
void TrackVelocity(float dx, float dy)
{
    const auto now = Clock::now();
    const float seconds = std::chrono::duration<float>(now - g_state.moveTime).count();
    g_state.moveTime = now;
    if (seconds <= 0.f)
        return;
    const float weight = 1.f - std::exp(-seconds / VelocitySmoothingSeconds);
    g_state.velocityX += (dx / seconds - g_state.velocityX) * weight;
    g_state.velocityY += (dy / seconds - g_state.velocityY) * weight;
}

void StartPinch(SDL_FingerID finger, float x, float y)
{
    g_state.second = finger;
    g_state.secondDown = true;
    g_state.secondX = x;
    g_state.secondY = y;
    g_state.pinchDistance = Distance(g_state.x, g_state.y, x, y);
    g_state.phase = Phase::Pinch;
}

// A finger lands while one finger of a pinch is still down: that one becomes the first again and the
// pair pinches on, as when a finger lifts and lands again in a map.
void ResumePinch(SDL_FingerID finger, float x, float y)
{
    if (!g_state.firstDown)
    {
        g_state.first = g_state.second;
        g_state.firstDown = true;
        g_state.x = g_state.secondX;
        g_state.y = g_state.secondY;
    }
    StartPinch(finger, x, y);
}

void TurnWheelByPinch(SDL_Window* window)
{
    const float step = PinchStep * WindowHeight(window);
    const float distance = Distance(g_state.x, g_state.y, g_state.secondX, g_state.secondY);
    const int steps = static_cast<int>((distance - g_state.pinchDistance) / step);
    if (steps == 0)
        return;
    g_state.pinchDistance += static_cast<float>(steps) * step;
    PushWheel(window, steps);
}

// A finger moving off the point it landed on: a list under that point scrolls with it, anything else
// is dragged with the left button, as a mouse's press there and a move from it. The pointer is still
// where the finger landed (Down); it follows the finger from the next Tick on.
void StartDrag(SDL_Window* window)
{
    if (BeginUiTouchScroll(g_state.downX, g_state.downY))
    {
        g_state.phase = Phase::Scrolling;
        g_state.moveTime = g_state.downTime;
        TrackVelocity(g_state.x - g_state.downX, g_state.y - g_state.downY);
        MoveUiTouchScroll(g_state.downX, g_state.downY, g_state.x, g_state.y);
        return;
    }
    g_state.phase = Phase::Dragging;
    g_state.heldAtLanding = true;
    g_lastLeftPress = LeftPress::Drag;
    PushButton(window, SDL_BUTTON_LEFT, true, 1, g_state.downX, g_state.downY);
}

void Scroll(float fromX, float fromY)
{
    TrackVelocity(g_state.x - fromX, g_state.y - fromY);
    MoveUiTouchScroll(fromX, fromY, g_state.x, g_state.y);
}

// A flick lifts the finger while it still moves: the list glides on at its speed.
void EndScroll(bool cancelled)
{
    const bool flick = !cancelled && Clock::now() - g_state.moveTime < FlickHoldTime;
    EndUiTouchScroll(g_state.x, g_state.y, flick ? g_state.velocityX : 0.f, flick ? g_state.velocityY : 0.f);
}

void FinishIfLifted()
{
    if (!g_state.firstDown && !g_state.secondDown)
        g_state.phase = Phase::None;
}

void MoveFirst(float x, float y, SDL_Window* window)
{
    const float fromX = g_state.x;
    const float fromY = g_state.y;
    g_state.x = x;
    g_state.y = y;
    switch (g_state.phase)
    {
    case Phase::Pending:
        if (Distance(x, y, g_state.downX, g_state.downY) > TapSlop * WindowHeight(window))
            StartDrag(window);
        return;
    case Phase::Scrolling:
        Scroll(fromX, fromY);
        return;
    case Phase::Dragging:
        if (!g_state.heldAtLanding)
            MovePointer(window, x, y);
        return;
    case Phase::LongPress:
        MovePointer(window, x, y);
        return;
    case Phase::Pinch:
        TurnWheelByPinch(window);
        return;
    default:
        return;
    }
}

void LiftFirst(float x, float y, bool cancelled, SDL_Window* window)
{
    const float fromX = g_state.x;
    const float fromY = g_state.y;
    g_state.firstDown = false;
    g_state.x = x;
    g_state.y = y;
    switch (Gesture::OnLift(g_state.phase, cancelled, g_state.stoppedGlide))
    {
    case Gesture::Lift::Click:
        MovePointer(window, x, y);
        Tap(window);
        break;
    case Gesture::Lift::EndScroll:
        // A lift where the last move left the finger is no move: it must not count as a fresh one.
        if (x != fromX || y != fromY)
            Scroll(fromX, fromY);
        EndScroll(cancelled);
        break;
    case Gesture::Lift::Drop:
        MovePointer(window, x, y);
        PushButton(window, SDL_BUTTON_LEFT, false, 1, x, y);
        break;
    case Gesture::Lift::CancelDrag:
        CancelDrag(window);
        break;
    case Gesture::Lift::ReleaseRight:
        PushButton(window, SDL_BUTTON_RIGHT, false, 1, x, y);
        break;
    case Gesture::Lift::EndPinch:
        g_state.phase = Phase::Ending;
        break;
    case Gesture::Lift::Nothing:
        break;
    }
    g_state.heldAtLanding = false;
    if (g_state.phase != Phase::Ending)
        g_state.phase = g_state.secondDown ? Phase::Ending : Phase::None;
    FinishIfLifted();
}

void StartTouch(SDL_FingerID finger, float x, float y, SDL_Window* window)
{
    g_state = State{.lastTapTime = g_state.lastTapTime, .lastTapX = g_state.lastTapX, .lastTapY = g_state.lastTapY};
    g_state.phase = Phase::Pending;
    g_state.first = finger;
    g_state.firstDown = true;
    g_state.downX = g_state.x = x;
    g_state.downY = g_state.y = y;
    g_state.downTime = Clock::now();
    g_state.stoppedGlide = StopUiTouchScrollGlide();
    // The pointer goes where the finger is at once: what is under it shows its hover (an item's tooltip).
    MovePointer(window, x, y);
    g_fingerPlaced = true;
}
} // namespace

void Down(SDL_FingerID finger, float x, float y, SDL_Window* window)
{
    switch (Gesture::OnLanding(g_state.phase))
    {
    case Gesture::Landing::Start:
        StartTouch(finger, x, y, window);
        return;
    case Gesture::Landing::Pinch:
        StartPinch(finger, x, y);
        return;
    case Gesture::Landing::CancelDragThenPinch:
        CancelDrag(window);
        StartPinch(finger, x, y);
        return;
    case Gesture::Landing::ResumePinch:
        ResumePinch(finger, x, y);
        return;
    case Gesture::Landing::Ignore:
        return;
    }
}

void Move(SDL_FingerID finger, float x, float y, SDL_Window* window)
{
    if (g_state.firstDown && finger == g_state.first)
    {
        MoveFirst(x, y, window);
        return;
    }
    if (g_state.secondDown && finger == g_state.second)
    {
        g_state.secondX = x;
        g_state.secondY = y;
        if (g_state.phase == Phase::Pinch)
            TurnWheelByPinch(window);
    }
}

void Up(SDL_FingerID finger, float x, float y, bool cancelled, SDL_Window* window)
{
    if (g_state.firstDown && finger == g_state.first)
    {
        LiftFirst(x, y, cancelled, window);
        return;
    }
    if (g_state.secondDown && finger == g_state.second)
    {
        g_state.secondDown = false;
        if (g_state.phase == Phase::Pinch)
            g_state.phase = Phase::Ending;
        FinishIfLifted();
    }
}

void Tick(SDL_Window* window, bool carryingItem)
{
    if (g_state.heldAtLanding)
    {
        g_state.heldAtLanding = false;
        MovePointer(window, g_state.x, g_state.y);
    }
    const auto held = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_state.downTime);
    if (!Gesture::IsLongPressDue(g_state.phase, held, carryingItem))
        return;
    g_state.phase = Phase::LongPress;
    // Where the pointer is: a still finger has not moved it from where it landed.
    PushButton(window, SDL_BUTTON_RIGHT, true, 1, g_state.downX, g_state.downY);
}

void Reset(SDL_Window* window)
{
    if (g_state.phase == Phase::Scrolling)
        EndScroll(true);
    if (window != nullptr)
        ReleaseButtons(window);
    g_state = State{};
    g_lastLeftPress = LeftPress::NotFinger;
}

bool Rest(SDL_Window* window)
{
    if (g_state.phase != Phase::None)
        return false;
    if (window != nullptr && g_fingerPlaced)
        LeaveWindow(window);
    g_fingerPlaced = false;
    return true;
}

void ForgetFinger()
{
    g_fingerPlaced = false;
    g_lastLeftPress = LeftPress::NotFinger;
}

bool IsButtonHeld(int sdlButton)
{
    return sdlButton >= 0 && sdlButton < ButtonSlots && g_state.buttons[sdlButton];
}

LeftPress LastLeftPress()
{
    return g_lastLeftPress;
}

SDL_FPoint MoveFromLanding()
{
    return {g_state.x - g_state.downX, g_state.y - g_state.downY};
}
} // namespace Core::Input::Touch::Pointer
