#pragma once

#include <SDL3/SDL_scancode.h>

namespace Core::Input
{
SDL_Scancode VkToScancode(int virtualKey);
void RecordLeftMouseButtonPressEdge();
void ClearLeftMouseButtonPressEdge();

// A key pressed since the last key scan (CNewKeyInput::ScanAsyncKeyState) counts as down for that scan,
// even if it is up again by then: a short tap during a long frame (a file download, a hitch) was lost,
// as a fast click was before RecordLeftMouseButtonPressEdge. The main loop records SDL's key presses,
// the scan clears them.
void RecordKeyPressEdge(SDL_Scancode scancode);
void ClearKeyPressEdges();

// Portable replacement for the Win32 "is this key currently down" check
// (HIBYTE(GetAsyncKeyState(vk)) == 128 / & 0x8000), backed by SDL keyboard
// state. The argument is a Win32 virtual-key code (VK_*) or an ASCII letter
// or digit, matching what the existing call sites already pass.
bool IsKeyDown(int virtualKey);

// IsKeyDown for every virtual-key code 0..255 at once, reading SDL's keyboard, modifier and mouse state a single
// time for all of them (the key scan asked 256 times a frame).
void ReadKeysDown(bool (&down)[256]);

// Whether Caps Lock is on (SDL's modifier state; in a browser it is known after the first key press).
bool IsCapsLockOn();

#ifdef _EDITOR
// Whether a key or button (a virtual-key code as IsKeyDown takes) is with a
// window that is not the game's, so that IsKeyDown leaves it out. Set by the
// editor for its own windows; without one every key is the game's.
using OtherWindowQuery = bool (*)(int virtualKey);
void SetOtherWindowQuery(OtherWindowQuery query);
#endif
}
