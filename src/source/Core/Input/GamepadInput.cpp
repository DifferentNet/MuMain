#include "stdafx.h"
#include "Core/Input/GamepadInput.h"

#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadStick.h"
#include "Core/Utilities/Log/MuLogger.h"

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebGamepad.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Core::Input::Gamepad
{
namespace
{
// Pads served at once; a fifth is ignored until one goes.
constexpr int MaxPads = 4;
// A stick moved this far from its centre, as a share of its throw, takes the game back from the
// mouse: a drifting one wanders well short of it.
constexpr float TakeOverLean = 0.5f;
// A trigger presses past this share of its pull and lets go under the second: no chatter at the edge.
constexpr float TriggerPressPull = 0.5f;
constexpr float TriggerReleasePull = 0.3f;
constexpr float AxisRange = 32767.f;
// The axes a poll reads. The triggers keep to their events: SDL reads a browser pad's trigger that
// has not moved yet as half pulled.
constexpr SDL_GamepadAxis StickAxes[] = {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX,
                                         SDL_GAMEPAD_AXIS_RIGHTY};

struct Pad
{
    SDL_Gamepad* handle = nullptr;
    SDL_JoystickID id = 0;
    std::array<bool, ButtonCount> held{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes{};
    // SDL's axis values at the last poll: only a change since then counts, so a stick held when the
    // window lost the focus stays let go.
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> polled{};
};

std::array<Pad, MaxPads> g_pads{};
std::array<ButtonState, ButtonCount> g_buttons{};
bool g_usedLast = false;
// Off: no pad is opened and the pads' events go unread (Options -> Interface -> Gamepad).
bool g_enabled = true;
bool g_started = false;

std::optional<Button> FromSdlButton(int button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return Button::A;
    case SDL_GAMEPAD_BUTTON_EAST:
        return Button::B;
    case SDL_GAMEPAD_BUTTON_WEST:
        return Button::X;
    case SDL_GAMEPAD_BUTTON_NORTH:
        return Button::Y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return Button::LeftShoulder;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return Button::RightShoulder;
    case SDL_GAMEPAD_BUTTON_BACK:
        return Button::View;
    case SDL_GAMEPAD_BUTTON_START:
        return Button::Menu;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return Button::LeftStick;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return Button::RightStick;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return Button::DpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return Button::DpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return Button::DpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return Button::DpadRight;
    default:
        // The guide button belongs to the system (Steam, the Xbox overlay), the rest are extras.
        return std::nullopt;
    }
}

Pad* FindPad(SDL_JoystickID id)
{
    for (Pad& pad : g_pads)
    {
        if (pad.handle != nullptr && pad.id == id)
            return &pad;
    }
    return nullptr;
}

bool AnyPadHolds(int button)
{
    return std::any_of(g_pads.begin(), g_pads.end(),
                       [button](const Pad& pad) { return pad.handle != nullptr && pad.held[button]; });
}

// One pad's button changed: the shared state follows, a press counts when no other pad held it.
void SetPadButton(Pad& pad, Button button, bool down)
{
    const auto index = static_cast<int>(button);
    if (pad.held[index] == down)
        return;
    const bool wasHeld = g_buttons[index].held;
    pad.held[index] = down;
    g_buttons[index].held = AnyPadHolds(index);
    if (!wasHeld && g_buttons[index].held)
        ++g_buttons[index].presses;
    if (down)
        g_usedLast = true;
}

void OnTrigger(Pad& pad, Button trigger, float pull)
{
    const bool held = pad.held[static_cast<int>(trigger)];
    if (!held && pull > TriggerPressPull)
        SetPadButton(pad, trigger, true);
    else if (held && pull < TriggerReleasePull)
        SetPadButton(pad, trigger, false);
}

bool IsLeftStickAxis(int axis)
{
    return axis == SDL_GAMEPAD_AXIS_LEFTX || axis == SDL_GAMEPAD_AXIS_LEFTY;
}

// How far the stick an axis belongs to leans from its centre, before any dead zone (0 .. 1 and a
// little more on a diagonal).
float RawLean(const Pad& pad, int axis)
{
    if (IsLeftStickAxis(axis))
        return std::hypot(pad.axes[SDL_GAMEPAD_AXIS_LEFTX], pad.axes[SDL_GAMEPAD_AXIS_LEFTY]);
    return std::hypot(pad.axes[SDL_GAMEPAD_AXIS_RIGHTX], pad.axes[SDL_GAMEPAD_AXIS_RIGHTY]);
}

// An axis's new place: a stick -1 .. 1, a trigger 0 .. 1.
void SetAxis(Pad& pad, int axis, float position)
{
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT)
        return;
    pad.axes[axis] = position;
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
        OnTrigger(pad, Button::LeftTrigger, position);
    else if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
        OnTrigger(pad, Button::RightTrigger, position);
    else if (RawLean(pad, axis) > TakeOverLean)
        g_usedLast = true;
}

float AxisPosition(Sint16 value)
{
    return std::clamp(static_cast<float>(value) / AxisRange, -1.f, 1.f);
}

void ReleasePad(Pad& pad)
{
    for (int button = 0; button < ButtonCount; ++button)
        SetPadButton(pad, static_cast<Button>(button), false);
    pad.axes.fill(0.f);
}

// A press and its release at once, of a button no pad holds: the reader sees one tap.
void Tap(Button button)
{
    ButtonState& state = g_buttons[static_cast<int>(button)];
    if (state.held)
        return;
    ++state.presses;
    g_usedLast = true;
}

#ifdef __EMSCRIPTEN__
// The buttons of the browser's "standard" layout, in its order (mu::web::HeldStandardButtons()).
constexpr Button StandardButtons[] = {
    Button::A,
    Button::B,
    Button::X,
    Button::Y,
    Button::LeftShoulder,
    Button::RightShoulder,
    Button::LeftTrigger,
    Button::RightTrigger,
    Button::View,
    Button::Menu,
    Button::LeftStick,
    Button::RightStick,
    Button::DpadUp,
    Button::DpadDown,
    Button::DpadLeft,
    Button::DpadRight,
};

// The press that woke a pad in the browser came with it (see WebGamepad.h): it counts as a tap. A
// hold that began with it is seen from the next press.
void TapWakingPress()
{
    const std::uint32_t held = mu::web::HeldStandardButtons(TriggerPressPull);
    for (std::size_t index = 0; index < std::size(StandardButtons); ++index)
    {
        if ((held & (1u << index)) != 0)
            Tap(StandardButtons[index]);
    }
}
#endif

void AddPad(SDL_JoystickID id)
{
    if (!g_enabled || FindPad(id) != nullptr)
        return;
    auto free = std::find_if(g_pads.begin(), g_pads.end(), [](const Pad& pad) { return pad.handle == nullptr; });
    if (free == g_pads.end())
        return;
    SDL_Gamepad* handle = SDL_OpenGamepad(id);
    if (handle == nullptr)
    {
        mu::log::Get("input")->warn("Gamepad {} could not be opened: {}", id, SDL_GetError());
        return;
    }
    *free = Pad{};
    free->handle = handle;
    free->id = id;
    const char* name = SDL_GetGamepadName(handle);
    mu::log::Get("input")->info("Gamepad connected: {}", name != nullptr ? name : "unnamed");
#ifdef __EMSCRIPTEN__
    TapWakingPress();
#endif
}

void ClosePad(Pad& pad)
{
    ReleasePad(pad);
    SDL_CloseGamepad(pad.handle);
    pad = Pad{};
    if (!IsConnected())
        g_usedLast = false;
}

void RemovePad(SDL_JoystickID id)
{
    Pad* pad = FindPad(id);
    if (pad == nullptr)
        return;
    ClosePad(*pad);
    mu::log::Get("input")->info("Gamepad disconnected");
}

// The pads plugged in now: SDL reports them as added only once, when its gamepad support opens.
void AddPluggedPads()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr)
        return;
    for (int i = 0; i < count; ++i)
        AddPad(ids[i]);
    SDL_free(ids);
}

// A press of another device: the pad is no longer the one in use. The pad's own pointer clicks
// (pushed mouse events) do not count, nor does the mouse merely moving.
bool IsOtherDevicePress(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_FINGER_DOWN:
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        return event.button.which != Pointer::MouseId;
    case SDL_EVENT_MOUSE_WHEEL:
        return event.wheel.which != Pointer::MouseId;
    default:
        return false;
    }
}
} // namespace

void Init()
{
    // The pad's pointer moves the mouse by warping it (GamepadPointer). With the cursor hidden, SDL
    // would take a few warps in a row for a game that locks the mouse, switch to relative mode and,
    // in a browser, ask for the pointer lock.
    SDL_SetHint(SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, "0");
    g_started = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!g_started)
        mu::log::Get("input")->warn("Gamepads unavailable: {}", SDL_GetError());
}

void SetEnabled(bool enabled)
{
    if (enabled == g_enabled)
        return;
    g_enabled = enabled;
    if (enabled)
    {
        if (g_started)
            AddPluggedPads();
    }
    else
    {
        for (Pad& pad : g_pads)
        {
            if (pad.handle != nullptr)
                ClosePad(pad);
        }
    }
    mu::log::Get("input")->info("Gamepad {}", enabled ? "on" : "off");
}

bool IsEnabled()
{
    return g_enabled;
}

void ProcessEvent(const SDL_Event& event)
{
    if (IsOtherDevicePress(event))
    {
        g_usedLast = false;
        return;
    }
    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
        AddPad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        RemovePad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (Pad* pad = FindPad(event.gbutton.which))
        {
            if (const std::optional<Button> button = FromSdlButton(event.gbutton.button))
                SetPadButton(*pad, *button, event.gbutton.down);
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (Pad* pad = FindPad(event.gaxis.which))
            SetAxis(*pad, event.gaxis.axis, AxisPosition(event.gaxis.value));
        break;
    default:
        break;
    }
}

void Poll()
{
    for (Pad& pad : g_pads)
    {
        if (pad.handle == nullptr)
            continue;
        for (const SDL_GamepadAxis axis : StickAxes)
        {
            const Sint16 value = SDL_GetGamepadAxis(pad.handle, axis);
            if (value == pad.polled[axis])
                continue;
            pad.polled[axis] = value;
            SetAxis(pad, axis, AxisPosition(value));
        }
    }
}

bool IsConnected()
{
    return std::any_of(g_pads.begin(), g_pads.end(), [](const Pad& pad) { return pad.handle != nullptr; });
}

bool WasUsedLast()
{
    return g_usedLast;
}

const ButtonState& GetButton(Button button)
{
    return g_buttons[static_cast<int>(button)];
}

namespace
{
// The stick pushed furthest among the pads.
StickPosition Stick(SDL_GamepadAxis xAxis, SDL_GamepadAxis yAxis)
{
    StickPosition furthest;
    float furthestLength = 0.f;
    for (const Pad& pad : g_pads)
    {
        if (pad.handle == nullptr)
            continue;
        const StickPosition position = PastDeadZone(pad.axes[xAxis], pad.axes[yAxis]);
        const float length = Lean(position);
        if (length > furthestLength)
        {
            furthest = position;
            furthestLength = length;
        }
    }
    return furthest;
}
} // namespace

StickPosition LeftStick()
{
    return Stick(SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY);
}

StickPosition RightStick()
{
    return Stick(SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY);
}

void Reset()
{
    for (Pad& pad : g_pads)
    {
        if (pad.handle != nullptr)
            ReleasePad(pad);
    }
}
} // namespace Core::Input::Gamepad
