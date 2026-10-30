// A game controller laid out as an Xbox pad: an Xbox Series or One pad, the Steam Deck's controls,
// and any pad SDL maps to that layout (the browser's "standard" gamepads included). SDL names the
// buttons by place, so a DualSense's cross is A here; its own glyphs are for later.
//
// This is the device's state only: what the buttons and sticks do in the game is
// UI/HUD/GamepadControls, which reads it once a frame. Any connected pad drives it; two pads at once
// press the same buttons.
#pragma once

#include <cstdint>

union SDL_Event;

namespace Core::Input::Gamepad
{
enum class Button : std::uint8_t
{
    A,
    B,
    X,
    Y,
    LeftShoulder,
    RightShoulder,
    // The triggers count as buttons once pulled past half way.
    LeftTrigger,
    RightTrigger,
    View,
    Menu,
    LeftStick,
    RightStick,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Count,
};
inline constexpr int ButtonCount = static_cast<int>(Button::Count);

// The counters only grow: a reader compares them with the values it saw last, so a press and
// release within one frame is not lost.
struct ButtonState
{
    bool held = false;
    std::uint32_t presses = 0;
};

// A stick's position past its dead zone (GamepadStick.h), each axis -1 .. 1, x to the right and y
// down; (0, 0) inside the dead zone. The length grows from 0 at the dead zone's edge to 1 at the rim.
struct StickPosition
{
    float x = 0.f;
    float y = 0.f;
};

// Opens SDL's gamepad support, once SDL's video is up. A failure leaves the game without pads.
// SDL then reports the pads already plugged in as added; in a browser a pad shows only once one of
// its buttons is pressed (the page may not see it before), and that press counts as a tap.
void Init();

// The player's switch (Options -> Interface -> Gamepad, on unless config.ini says off): off closes
// the pads and leaves their buttons and sticks unread, for a pad that Steam Input or the handheld's
// own software already turns into a mouse and keys. On opens the pads plugged in. On to start with.
void SetEnabled(bool enabled);
[[nodiscard]] bool IsEnabled();

// Every event from the main loop: pads coming and going, their buttons and sticks, and the other
// devices' presses (a key, a mouse button or a finger makes the pad no longer the device used last).
void ProcessEvent(const SDL_Event& event);

// Once a frame, before the reads: takes the sticks where SDL holds them now. SDL keeps the first
// place an axis reports as its rest and sends no event for it, so a quick first flick held still
// would otherwise go unseen until the stick moved again.
void Poll();

[[nodiscard]] bool IsConnected();
// Whether a pad pressed a button or pushed a stick half way out since the last key, mouse button or
// finger press. A worn stick drifting near its centre does not take the game from the mouse.
[[nodiscard]] bool WasUsedLast();

[[nodiscard]] const ButtonState& GetButton(Button button);
[[nodiscard]] StickPosition LeftStick();
[[nodiscard]] StickPosition RightStick();

// Lets go of every button and centres the sticks: the window lost the focus, and the pad's
// releases go elsewhere.
void Reset();
} // namespace Core::Input::Gamepad
