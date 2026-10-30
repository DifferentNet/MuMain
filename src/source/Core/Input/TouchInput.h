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

#include <SDL3/SDL_rect.h>

#include <cstdint>
#include <optional>

union SDL_Event;
struct SDL_Window;

namespace Core::Input::Touch
{
// The id the hit test answers when no control is under the point.
inline constexpr int NoControl = -1;
// Controls have ids 0 .. MaxControls - 1.
inline constexpr int MaxControls = 96;

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
// Whether the mouse carries an item (picked up from a bag). Set by the inventory.
using CarryQuery = bool (*)();
// Lets the focused text field go, so keys reach the hotkeys again. Set by the main loop.
using EndTyping = void (*)();

// Whether fingers are routed here at all. Off, SDL's own finger-to-mouse emulation stays as it is.
void SetEnabled(bool enabled);
[[nodiscard]] bool IsEnabled();

void SetHitTest(HitTest hitTest);
// While the mouse carries an item a finger held still does not click the right button: that would put
// the item back in the bag, and the finger could no longer drag it.
void SetCarryQuery(CarryQuery isCarrying);
// TapKey() ends typing first, so a control's key works while a text field (the chat) is focused.
void SetEndTyping(EndTyping endTyping);

// Finger events from the main loop, and its mouse button presses: one of the mouse or the pad takes
// the pointer from where a finger left it (RestPointer()). True when the event was a finger's.
bool ProcessEvent(const SDL_Event& event, SDL_Window* window);

// Once a pass of the main loop, before its events: the long press and a drag's pointer catching up
// with the finger. The buttons they push are then among that pass's events, so the window's UI and
// the game see them in the same frame as the key-state scan does.
void Tick(SDL_Window* window);

// Once per rendered frame, before the key-state scan: keys tapped by controls.
void BeginFrame(SDL_Window* window);

// Lets go of every finger: the mouse buttons they held (released off the window, so nothing takes a
// drop), the controls, the pinch.
void Reset();

// Takes the pointer a lifted finger left behind off the window (Pointer::Rest): for a control that
// opens or shuts a window, where the old spot would show a tooltip nobody asked for. False while a
// finger still plays the mouse: call again once it lifts.
bool RestPointer();

[[nodiscard]] const ControlFinger& Control(int controlId);

// A key a control holds down, as a virtual-key code (VK_*, or an ASCII letter or digit).
void SetKeyHeld(int virtualKey, bool held);
// A key pressed and released by a control: held for one key-state scan. Any key but Enter and Esc,
// which a text field takes for itself, ends typing first (SetEndTyping()).
void TapKey(int virtualKey);
[[nodiscard]] bool IsKeyHeld(int virtualKey);

// Whether the mouse-playing finger holds this SDL mouse button (SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT).
[[nodiscard]] bool IsMouseButtonHeld(int sdlButton);

// Where the finger playing the mouse is while lifting it would tap there (Pointer::TapPoint()).
[[nodiscard]] std::optional<SDL_FPoint> TapPoint();
} // namespace Core::Input::Touch
