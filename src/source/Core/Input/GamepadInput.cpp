#include "stdafx.h"
#include "Core/Input/GamepadInput.h"

#include "Core/Input/GamepadPointer.h"
#include "Core/Utilities/Log/MuLogger.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace Core::Input::Gamepad
{
namespace
{
// Pads served at once; a fifth is ignored until one goes.
constexpr int MaxPads = 4;
// A stick this close to its centre, as a share of its throw, is at rest: worn sticks drift.
constexpr float StickDeadZone = 0.2f;
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
    // window lost the focus stays let go, and a worn one resting past the dead zone does not take
    // the game back from the mouse.
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> polled{};
};

std::array<Pad, MaxPads> g_pads{};
std::array<ButtonState, ButtonCount> g_buttons{};
bool g_usedLast = false;

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

StickPosition PastDeadZone(float x, float y)
{
    const float length = std::hypot(x, y);
    if (length <= StickDeadZone)
        return {};
    const float scaled = std::min(1.f, (length - StickDeadZone) / (1.f - StickDeadZone));
    return {x * scaled / length, y * scaled / length};
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
    else if (std::abs(position) > StickDeadZone)
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

void AddPad(SDL_JoystickID id)
{
    if (FindPad(id) != nullptr)
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
}

void RemovePad(SDL_JoystickID id)
{
    Pad* pad = FindPad(id);
    if (pad == nullptr)
        return;
    ReleasePad(*pad);
    SDL_CloseGamepad(pad->handle);
    *pad = Pad{};
    mu::log::Get("input")->info("Gamepad disconnected");
    if (!IsConnected())
        g_usedLast = false;
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
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
        mu::log::Get("input")->warn("Gamepads unavailable: {}", SDL_GetError());
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
        const float length = std::hypot(position.x, position.y);
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
