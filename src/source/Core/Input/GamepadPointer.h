// The mouse a gamepad plays: a stick moves the pointer and the pad's buttons click (UI/HUD/GamepadControls
// says which). The clicks are SDL mouse events pushed with a mouse id of their own, so they reach the
// window's UI and the game as a real mouse's do, and the pad knows them for its own.
//
// The pointer moves as the SDL mouse does (warped), so a real mouse or a finger moving it in between
// is followed from where they left it; a browser repeating a resting mouse's place is not.
#pragma once

#include <cstdint>

struct SDL_Window;

namespace Core::Input::Gamepad::Pointer
{
// The `which` of the mouse events the pad pushes (SDL_MouseID). SDL's own reserved ids are
// SDL_TOUCH_MOUSEID (-1) and SDL_PEN_MOUSEID (-2).
inline constexpr std::uint32_t MouseId = 0xFFFFFFFDu;

// Once a frame while a stick plays the pointer: x and y (-1 .. 1, past the stick's dead zone) push
// it that way, faster the further the stick leans; `precise` slows it (over a window's small
// buttons). `seconds` is the frame's length. After a resize or a turn of the phone it first puts the
// pointer at the same share of the new size (SetButton() too, so a click lands in the window).
void Move(SDL_Window* window, float x, float y, float seconds, bool precise);

// Puts the pointer in the middle of the window: over the character, clear of the windows.
void Centre(SDL_Window* window);

// Presses or lets go of an SDL mouse button (SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT) where the pointer is.
// A second left press soon after the first, close to it, is a double click.
void SetButton(SDL_Window* window, int sdlButton, bool down);

// Whether the pad holds this SDL mouse button: ORed into Core::Input::IsKeyDown(VK_LBUTTON/VK_RBUTTON),
// as pushed events leave SDL's own button state alone.
[[nodiscard]] bool IsButtonHeld(int sdlButton);

// Lets go of the buttons it holds.
void Release(SDL_Window* window);
} // namespace Core::Input::Gamepad::Pointer
