// The finger that plays the mouse (TouchInput.h): a tap clicks, a long press clicks the right button,
// a moving finger drags with the left button held, and a second finger turns the pair into a pinch
// that turns the mouse wheel. A finger moving over a scrollable list of the UI scrolls it instead, as
// on a phone (Core::Input::BeginUiTouchScroll()): a flick glides on, and a tap on a gliding list only
// stops it.
#pragma once

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_touch.h>

#include <cstdint>

struct SDL_Window;

namespace Core::Input::Touch::Pointer
{
// A finger that landed on no control, at a window pixel. A second finger pinches with the first
// (Gesture::OnLanding()); one landing while another gesture is under way is ignored.
void Down(SDL_FingerID finger, float x, float y, SDL_Window* window);
void Move(SDL_FingerID finger, float x, float y, SDL_Window* window);
// `cancelled`: the system took the finger away (a gesture of the browser's): no click, no drop.
void Up(SDL_FingerID finger, float x, float y, bool cancelled, SDL_Window* window);

// Once a pass of the main loop, before its events: a drag's pointer catches up with the finger, and
// a finger held still long enough turns into the right button, unless the mouse carries an item.
void Tick(SDL_Window* window, bool carryingItem);

// Releases the buttons the finger holds and forgets the gesture.
void Reset(SDL_Window* window);

// Takes the pointer a lifted finger left behind off the window, so nothing shows a hover for it: a
// window opening where the last tap was shows no tooltip of its own. False while a finger still plays
// the mouse: call again once it lifts. A pointer the mouse or the pad pressed with since is theirs and
// stays.
bool Rest(SDL_Window* window);

// The mouse or the pad's pointer pressed a button: the pointer is no longer where a finger left it,
// and the buttons are no longer a finger's (LastLeftPress()).
void ForgetFinger();

[[nodiscard]] bool IsButtonHeld(int sdlButton);

// How the last press of the left button came: a window that takes a mouse's click at once can ask a
// finger to tap twice, or let a finger's drag that only scrolled the view do nothing.
enum class LeftPress : std::uint8_t
{
    // The mouse or the pad pressed it last, or no finger has.
    NotFinger,
    // A finger tapped: pressed and let go where it lifted.
    Tap,
    // A finger moved off where it landed: pressed there, let go where it lifts.
    Drag,
};
[[nodiscard]] LeftPress LastLeftPress();

// How far the finger playing the mouse is from where it landed, in window pixels: which way a drag
// set off when it began.
[[nodiscard]] SDL_FPoint MoveFromLanding();
} // namespace Core::Input::Touch::Pointer
