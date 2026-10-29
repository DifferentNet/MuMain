#include "stdafx.h"
#include "Core/Input/TouchPointer.h"

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

// A finger held still this long clicks the right button (as Remote Desktop and Android's mouse
// emulation do), and holds it until the finger lifts.
constexpr auto LongPressTime = std::chrono::milliseconds(500);
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

enum class Phase : std::uint8_t
{
    // No finger plays the mouse.
    None,
    // Down, not yet a tap, a drag or a long press.
    Pending,
    // Moved: the left button is held and follows the finger.
    Dragging,
    // Held still: the right button is held.
    LongPress,
    // Two fingers: the wheel turns as they spread or close.
    Pinch,
    // The pinch's fingers are lifting: nothing more until both are up.
    Ending,
};

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
    Clock::time_point lastTapTime;
    float lastTapX = 0.f;
    float lastTapY = 0.f;
    std::array<bool, ButtonSlots> buttons{};
};

State g_state;

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

void PushButton(SDL_Window* window, Uint8 button, bool down, Uint8 clicks)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.timestamp = SDL_GetTicksNS();
    event.button.windowID = SDL_GetWindowID(window);
    event.button.which = SDL_TOUCH_MOUSEID;
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = clicks;
    event.button.x = g_state.x;
    event.button.y = g_state.y;
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

void ReleaseButtons(SDL_Window* window)
{
    for (Uint8 button = 0; button < ButtonSlots; ++button)
    {
        if (g_state.buttons[button])
            PushButton(window, button, false, 1);
    }
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
    PushButton(window, SDL_BUTTON_LEFT, true, clicks);
    PushButton(window, SDL_BUTTON_LEFT, false, clicks);
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

void FinishIfLifted()
{
    if (!g_state.firstDown && !g_state.secondDown)
        g_state.phase = Phase::None;
}

void MoveFirst(float x, float y, SDL_Window* window)
{
    g_state.x = x;
    g_state.y = y;
    switch (g_state.phase)
    {
    case Phase::Pending:
        if (Distance(x, y, g_state.downX, g_state.downY) <= TapSlop * WindowHeight(window))
            return;
        // The drag starts where the finger landed, as a mouse's press there and a move from it.
        g_state.phase = Phase::Dragging;
        PushButton(window, SDL_BUTTON_LEFT, true, 1);
        MovePointer(window, x, y);
        return;
    case Phase::Dragging:
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
    g_state.firstDown = false;
    g_state.x = x;
    g_state.y = y;
    switch (g_state.phase)
    {
    case Phase::Pending:
        if (!cancelled)
        {
            MovePointer(window, x, y);
            Tap(window);
        }
        break;
    case Phase::Dragging:
        MovePointer(window, x, y);
        PushButton(window, SDL_BUTTON_LEFT, false, 1);
        break;
    case Phase::LongPress:
        PushButton(window, SDL_BUTTON_RIGHT, false, 1);
        break;
    case Phase::Pinch:
        g_state.phase = Phase::Ending;
        break;
    default:
        break;
    }
    if (g_state.phase != Phase::Ending)
        g_state.phase = g_state.secondDown ? Phase::Ending : Phase::None;
    FinishIfLifted();
}
} // namespace

void Down(SDL_FingerID finger, float x, float y, SDL_Window* window)
{
    if (g_state.phase == Phase::Pending && !g_state.secondDown)
    {
        StartPinch(finger, x, y);
        return;
    }
    if (g_state.phase != Phase::None)
        return;

    g_state = State{.lastTapTime = g_state.lastTapTime, .lastTapX = g_state.lastTapX, .lastTapY = g_state.lastTapY};
    g_state.phase = Phase::Pending;
    g_state.first = finger;
    g_state.firstDown = true;
    g_state.downX = g_state.x = x;
    g_state.downY = g_state.y = y;
    g_state.downTime = Clock::now();
    // The pointer goes where the finger is at once: what is under it shows its hover (an item's tooltip).
    MovePointer(window, x, y);
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

void Tick(SDL_Window* window)
{
    if (g_state.phase != Phase::Pending || Clock::now() - g_state.downTime < LongPressTime)
        return;
    g_state.phase = Phase::LongPress;
    PushButton(window, SDL_BUTTON_RIGHT, true, 1);
}

void Reset(SDL_Window* window)
{
    if (window != nullptr)
        ReleaseButtons(window);
    g_state = State{};
}

bool IsButtonHeld(int sdlButton)
{
    return sdlButton >= 0 && sdlButton < ButtonSlots && g_state.buttons[sdlButton];
}
} // namespace Core::Input::Touch::Pointer
