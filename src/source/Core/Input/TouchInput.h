// Fingers on a touch screen (a phone or tablet in the browser): each finger is either on one of the
// on-screen controls (the joystick, a button) or plays the mouse.
//
// A finger that lands on a control belongs to that control until it lifts, so a thumb can hold the
// joystick while the other one presses buttons. The touch controls (UI/HUD/TouchControls) say which
// control is under a point and read the controls' fingers here once a frame.
//
// The first finger anywhere else plays the mouse: a tap clicks, a long press clicks the right button,
// a finger that moves drags with the left button held, and a second finger next to it turns the pair
// into a pinch that turns the mouse wheel (the camera's zoom). The clicks go through the same path as
// a real mouse's (the window's UI first, then the game).
//
// The controls press keys too (a menu button presses I for the inventory): such a key counts as held
// in Core::Input::IsKeyDown, so every hotkey works as from a keyboard.
#pragma once

#include <cstdint>

union SDL_Event;
struct SDL_Window;

namespace Core::Input::Touch
{
// The id the hit test answers when no control is under the point.
inline constexpr int NoControl = -1;
// Controls have ids 0 .. MaxControls - 1.
inline constexpr int MaxControls = 48;

// What a control's finger did. The counters only grow: a reader compares them with the values it saw
// last, so a tap that pressed and released within one frame is not lost.
struct ControlFinger
{
    bool held = false;
    std::uint32_t presses = 0;
    std::uint32_t releases = 0;
    // Window pixels (the units of mouse events): where the finger landed and where it is now.
    float downX = 0.f;
    float downY = 0.f;
    float x = 0.f;
    float y = 0.f;
};

// The control under a window pixel, or NoControl. Set by the touch controls while they are shown.
using HitTest = int (*)(float x, float y);

// Whether fingers are routed here at all. Off, SDL's own finger-to-mouse emulation stays as it is.
void SetEnabled(bool enabled);
[[nodiscard]] bool IsEnabled();

void SetHitTest(HitTest hitTest);

// Finger events from the main loop. True when the event was a finger's.
bool ProcessEvent(const SDL_Event& event, SDL_Window* window);

// Once per rendered frame, before the key-state scan: the long press, and keys tapped by controls.
void BeginFrame(SDL_Window* window);

// Lets go of every finger: the mouse buttons they held, the controls, the pinch.
void Reset();

[[nodiscard]] const ControlFinger& Control(int controlId);

// A key a control holds down, as a virtual-key code (VK_*, or an ASCII letter or digit).
void SetKeyHeld(int virtualKey, bool held);
// A key pressed and released by a control: held for one key-state scan.
void TapKey(int virtualKey);
[[nodiscard]] bool IsKeyHeld(int virtualKey);

// Whether the mouse-playing finger holds this SDL mouse button (SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT).
[[nodiscard]] bool IsMouseButtonHeld(int sdlButton);
} // namespace Core::Input::Touch
