#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#include "Core/Input/GamepadInput.h"
#include "UI/Mobile/InterfaceMode.h"

#include "json.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

// `pad`: a virtual gamepad for testing the pad controls without one. It is SDL's own virtual
// joystick laid out as a gamepad, so its buttons and sticks reach the game the way a real pad's do:
// through SDL's gamepad events into Core::Input::Gamepad.
namespace
{
using App::Control::ErrorCode;
using App::Control::Request;
using nlohmann::json;

// Every gamepad button up to the d-pad and every axis. In this count SDL gives the virtual joystick
// the gamepad layout in order, so a joystick button or axis index is the gamepad one.
constexpr int VirtualButtons = SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
constexpr int VirtualAxes = SDL_GAMEPAD_AXIS_COUNT;
constexpr float AxisMax = 32767.f;

SDL_JoystickID g_virtualId = 0;
SDL_Joystick* g_virtual = nullptr;

struct NamedButton
{
    std::string_view name;
    SDL_GamepadButton button;
};

constexpr NamedButton Buttons[] = {
    {"a", SDL_GAMEPAD_BUTTON_SOUTH},
    {"b", SDL_GAMEPAD_BUTTON_EAST},
    {"x", SDL_GAMEPAD_BUTTON_WEST},
    {"y", SDL_GAMEPAD_BUTTON_NORTH},
    {"view", SDL_GAMEPAD_BUTTON_BACK},
    {"menu", SDL_GAMEPAD_BUTTON_START},
    {"ls", SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"rs", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"up", SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"down", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"left", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
};

struct NamedAxis
{
    std::string_view name;
    SDL_GamepadAxis axis;
};

constexpr NamedAxis Axes[] = {
    {"leftx", SDL_GAMEPAD_AXIS_LEFTX},         {"lefty", SDL_GAMEPAD_AXIS_LEFTY},
    {"rightx", SDL_GAMEPAD_AXIS_RIGHTX},       {"righty", SDL_GAMEPAD_AXIS_RIGHTY},
    {"lt", SDL_GAMEPAD_AXIS_LEFT_TRIGGER},     {"rt", SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
};

std::optional<SDL_GamepadButton> FindButton(std::string_view name)
{
    for (const NamedButton& named : Buttons)
    {
        if (named.name == name)
            return named.button;
    }
    return std::nullopt;
}

std::optional<SDL_GamepadAxis> FindAxis(std::string_view name)
{
    for (const NamedAxis& named : Axes)
    {
        if (named.name == name)
            return named.axis;
    }
    return std::nullopt;
}

bool IsTrigger(SDL_GamepadAxis axis)
{
    return axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
}

// A stick's place -1 .. 1, or a trigger's pull 0 .. 1, as the joystick axis value. SDL reads a
// gamepad trigger from the whole joystick range: its lowest value is let go, its highest pulled.
Sint16 JoystickValue(SDL_GamepadAxis axis, double position)
{
    if (IsTrigger(axis))
    {
        const double pull = std::clamp(position, 0.0, 1.0);
        return static_cast<Sint16>(-32768.0 + pull * 65535.0);
    }
    return static_cast<Sint16>(std::clamp(position, -1.0, 1.0) * AxisMax);
}

std::string Error(const Request& request, ErrorCode code, std::string_view message)
{
    return App::Control::EncodeError(request.EncodedId(), code, message);
}

std::string StateResult(const Request& request)
{
    json result;
    result["enabled"] = Core::Input::Gamepad::IsEnabled();
    result["off_when_put_down"] = Core::Input::Gamepad::TurnsOffWhenPutDown();
    result["connected"] = Core::Input::Gamepad::IsConnected();
    result["used_last"] = Core::Input::Gamepad::WasUsedLast();
    result["in_use"] = UI::Mobile::IsGamepadInUse();
    result["virtual"] = g_virtual != nullptr;
    return App::Control::EncodeResult(request.EncodedId(), result.dump());
}

std::string Attach(const Request& request)
{
    if (g_virtual != nullptr)
        return StateResult(request);
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = VirtualAxes;
    desc.nbuttons = VirtualButtons;
    desc.name = "Control socket pad";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    if (id == 0)
        return Error(request, ErrorCode::Failed, std::string("SDL refused the virtual pad: ") + SDL_GetError());
    SDL_Joystick* joystick = SDL_OpenJoystick(id);
    if (joystick == nullptr)
    {
        const std::string error = SDL_GetError();
        SDL_DetachVirtualJoystick(id);
        return Error(request, ErrorCode::Failed, "the virtual pad could not be opened: " + error);
    }
    g_virtualId = id;
    g_virtual = joystick;
    return StateResult(request);
}

void Detach()
{
    if (g_virtual == nullptr)
        return;
    SDL_CloseJoystick(g_virtual);
    SDL_DetachVirtualJoystick(g_virtualId);
    g_virtual = nullptr;
    g_virtualId = 0;
}

std::string SetButton(const Request& request, bool down)
{
    if (g_virtual == nullptr)
        return Error(request, ErrorCode::NotConnected, "no virtual pad: send `pad` with `action` attach first");
    std::string name;
    if (!request.GetString("button", name))
        return Error(request, ErrorCode::BadRequest, "`down` and `up` need a `button`");
    if (name == "lt" || name == "rt")
    {
        const SDL_GamepadAxis trigger = name == "lt" ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        SDL_SetJoystickVirtualAxis(g_virtual, trigger, JoystickValue(trigger, down ? 1.0 : 0.0));
        return StateResult(request);
    }
    const std::optional<SDL_GamepadButton> button = FindButton(name);
    if (!button)
    {
        return Error(request, ErrorCode::BadRequest,
                     "unknown button `" + name + "`; known: a b x y lb rb lt rt view menu ls rs up down left right");
    }
    SDL_SetJoystickVirtualButton(g_virtual, *button, down);
    return StateResult(request);
}

std::string SetAxis(const Request& request)
{
    if (g_virtual == nullptr)
        return Error(request, ErrorCode::NotConnected, "no virtual pad: send `pad` with `action` attach first");
    std::string name;
    double value = 0.0;
    if (!request.GetString("axis", name) || !request.GetDouble("value", value))
        return Error(request, ErrorCode::BadRequest, "`axis` needs an `axis` and a `value`");
    const std::optional<SDL_GamepadAxis> axis = FindAxis(name);
    if (!axis)
        return Error(request, ErrorCode::BadRequest, "unknown axis `" + name + "`; known: leftx lefty rightx righty lt rt");
    SDL_SetJoystickVirtualAxis(g_virtual, *axis, JoystickValue(*axis, value));
    return StateResult(request);
}
} // namespace

namespace App::Control::Commands
{
std::string Pad(const Request& request, std::unique_ptr<Act>& /*act*/)
{
    std::string action;
    if (!request.GetString("action", action))
        action = "state";
    if (action == "state")
        return StateResult(request);
    if (action == "attach")
        return Attach(request);
    if (action == "detach")
    {
        Detach();
        return StateResult(request);
    }
    if (action == "down" || action == "up")
        return SetButton(request, action == "down");
    if (action == "axis")
        return SetAxis(request);
    // The Options switch without its window: not saved to config.ini.
    if (action == "enable" || action == "disable")
    {
        Core::Input::Gamepad::SetEnabled(action == "enable");
        return StateResult(request);
    }
    return Error(request, ErrorCode::BadRequest,
                 "unknown `action` `" + action + "`; known: state attach detach down up axis enable disable");
}
} // namespace App::Control::Commands
