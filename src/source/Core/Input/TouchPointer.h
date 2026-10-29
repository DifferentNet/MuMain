// The finger that plays the mouse (TouchInput.h): a tap clicks, a long press clicks the right button,
// a moving finger drags with the left button held, and a second finger turns the pair into a pinch
// that turns the mouse wheel.
#pragma once

#include <SDL3/SDL_touch.h>

struct SDL_Window;

namespace Core::Input::Touch::Pointer
{
// A finger that landed on no control, at a window pixel. Ignored while another gesture is under way,
// unless it is the second finger of a pinch.
void Down(SDL_FingerID finger, float x, float y, SDL_Window* window);
void Move(SDL_FingerID finger, float x, float y, SDL_Window* window);
// `cancelled`: the system took the finger away (a gesture of the browser's): no click.
void Up(SDL_FingerID finger, float x, float y, bool cancelled, SDL_Window* window);

// Once a frame: a finger held still long enough turns into the right button.
void Tick(SDL_Window* window);

// Releases the buttons the finger holds and forgets the gesture.
void Reset(SDL_Window* window);

[[nodiscard]] bool IsButtonHeld(int sdlButton);
} // namespace Core::Input::Touch::Pointer
