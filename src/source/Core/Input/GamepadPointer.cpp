#include "stdafx.h"
#include "Core/Input/GamepadPointer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace Core::Input::Gamepad::Pointer
{
namespace
{
using Clock = std::chrono::steady_clock;

// The pointer's top speed, in window heights a second, with the stick at its rim; the speed grows
// with the square of the lean, so a slight one aims finely.
constexpr float TopSpeed = 1.2f;
// The share of that speed over a window, where the buttons are small.
constexpr float PreciseShare = 0.5f;
// A second left press this soon after the first, this close to it (a share of the window's
// height), is a double click.
constexpr auto DoubleClickTime = std::chrono::milliseconds(350);
constexpr float DoubleClickSlop = 0.02f;
// Where the pointer is, as the pad left it, may differ this much (pixels) from SDL's rounding
// before it counts as moved by another device.
constexpr float MovedElsewhere = 1.5f;
// How far from the window's exact centre the pointer starts (pixels).
constexpr float CentreOffset = 1.f;

constexpr int ButtonSlots = 4; // SDL_BUTTON_LEFT, _MIDDLE and _RIGHT index it

struct State
{
    // Fractions of a pixel carry over between frames: a slow lean still moves.
    float x = 0.f;
    float y = 0.f;
    bool placed = false;
    // Where another device (the mouse) last put the pointer: the browser repeats it now and then.
    float otherX = 0.f;
    float otherY = 0.f;
    bool otherKnown = false;
    std::array<bool, ButtonSlots> buttons{};
    Clock::time_point lastLeftPress{};
    float lastLeftX = 0.f;
    float lastLeftY = 0.f;
};
State g_state;

void WindowSize(SDL_Window* window, float& width, float& height)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window, &w, &h);
    width = static_cast<float>(std::max(w, 1));
    height = static_cast<float>(std::max(h, 1));
}

bool IsNear(float ax, float ay, float bx, float by)
{
    return std::abs(ax - bx) <= MovedElsewhere && std::abs(ay - by) <= MovedElsewhere;
}

void RememberOtherDevice(float x, float y)
{
    g_state.otherX = x;
    g_state.otherY = y;
    g_state.otherKnown = true;
}

// Takes SDL's pointer when another device moved it since the pad did. A browser also repeats a
// resting mouse's place now and then (a layout change under it) with the mouse still: the pointer
// goes back where the pad left it.
void FollowOtherDevices(SDL_Window* window)
{
    float x = 0.f;
    float y = 0.f;
    SDL_GetMouseState(&x, &y);
    if (g_state.placed && IsNear(x, y, g_state.x, g_state.y))
        return;
    if (g_state.placed && g_state.otherKnown && IsNear(x, y, g_state.otherX, g_state.otherY))
    {
        SDL_WarpMouseInWindow(window, g_state.x, g_state.y);
        return;
    }
    g_state.x = x;
    g_state.y = y;
    g_state.placed = true;
    RememberOtherDevice(x, y);
}

Uint8 Clicks(int sdlButton, float windowHeight)
{
    if (sdlButton != SDL_BUTTON_LEFT)
        return 1;
    const auto now = Clock::now();
    const bool second =
        now - g_state.lastLeftPress < DoubleClickTime &&
        std::hypot(g_state.x - g_state.lastLeftX, g_state.y - g_state.lastLeftY) < DoubleClickSlop * windowHeight;
    g_state.lastLeftPress = second ? Clock::time_point{} : now;
    g_state.lastLeftX = g_state.x;
    g_state.lastLeftY = g_state.y;
    return second ? 2 : 1;
}

void PushButton(SDL_Window* window, int sdlButton, bool down, Uint8 clicks)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.timestamp = SDL_GetTicksNS();
    event.button.windowID = SDL_GetWindowID(window);
    event.button.which = MouseId;
    event.button.button = static_cast<Uint8>(sdlButton);
    event.button.down = down;
    event.button.clicks = clicks;
    event.button.x = g_state.x;
    event.button.y = g_state.y;
    SDL_PushEvent(&event);
}
} // namespace

void Move(SDL_Window* window, float x, float y, float seconds, bool precise)
{
    if (window == nullptr)
        return;
    FollowOtherDevices(window);
    const float lean = std::hypot(x, y);
    if (lean <= 0.f)
        return;
    float width = 0.f;
    float height = 0.f;
    WindowSize(window, width, height);
    const float speed = TopSpeed * height * lean * (precise ? PreciseShare : 1.f);
    g_state.x = std::clamp(g_state.x + x * speed * seconds, 0.f, width - 1.f);
    g_state.y = std::clamp(g_state.y + y * speed * seconds, 0.f, height - 1.f);
    SDL_WarpMouseInWindow(window, g_state.x, g_state.y);
}

void Centre(SDL_Window* window)
{
    if (window == nullptr)
        return;
    float mouseX = 0.f;
    float mouseY = 0.f;
    SDL_GetMouseState(&mouseX, &mouseY);
    RememberOtherDevice(mouseX, mouseY);
    float width = 0.f;
    float height = 0.f;
    WindowSize(window, width, height);
    // A pixel off the exact centre: SDL takes two warps there in a row, with the cursor hidden,
    // for a game that locks the mouse, and would switch to relative mode.
    g_state.x = width / 2.f + CentreOffset;
    g_state.y = height / 2.f + CentreOffset;
    g_state.placed = true;
    SDL_WarpMouseInWindow(window, g_state.x, g_state.y);
}

void SetButton(SDL_Window* window, int sdlButton, bool down)
{
    if (window == nullptr || sdlButton < 0 || sdlButton >= ButtonSlots || g_state.buttons[sdlButton] == down)
        return;
    FollowOtherDevices(window);
    float width = 0.f;
    float height = 0.f;
    WindowSize(window, width, height);
    const Uint8 clicks = down ? Clicks(sdlButton, height) : 1;
    g_state.buttons[sdlButton] = down;
    PushButton(window, sdlButton, down, clicks);
}

bool IsButtonHeld(int sdlButton)
{
    return sdlButton >= 0 && sdlButton < ButtonSlots && g_state.buttons[sdlButton];
}

void Release(SDL_Window* window)
{
    for (int button = 0; button < ButtonSlots; ++button)
    {
        if (!g_state.buttons[button])
            continue;
        if (window != nullptr)
            PushButton(window, button, false, 1);
        g_state.buttons[button] = false;
    }
}
} // namespace Core::Input::Gamepad::Pointer
