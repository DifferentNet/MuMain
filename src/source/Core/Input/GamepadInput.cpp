#include "stdafx.h"
#include "Core/Input/GamepadInput.h"

#include "Core/Input/GamepadBattery.h"
#include "Core/Input/GamepadLayout.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/GamepadRumble.h"
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
#include <utility>

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
    PadKind kind = PadKind::Xbox;
    std::array<bool, ButtonCount> held{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes{};
    // SDL's axis values at the last poll: only a change since then counts, so a stick held when the
    // window lost the focus stays let go.
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> polled{};
#ifndef __EMSCRIPTEN__
    // When its battery is worth a line (NoteBattery); a browser tells no battery.
    BatteryWatch battery;
#endif
};

std::array<Pad, MaxPads> g_pads{};
std::array<ButtonState, ButtonCount> g_buttons{};
bool g_usedLast = false;
// Off: no pad is opened and the pads' events go unread (Options -> Interface -> Gamepad).
bool g_enabled = true;
// Turned off with the pad itself: it plays on until another device is pressed (SetEnabled()).
bool g_offWhenPutDown = false;
bool g_started = false;
bool g_rumble = true;
PadKind g_lastKind = PadKind::Xbox;
// A pad plugged in or out since TakeNews last asked (not the pads open at the start or by the switch).
PadNews g_news = PadNews::None;
// A pad's low battery percent since TakeBatteryNews last asked; -1 none. g_batteryNewsPad is the pad
// that told it: only that pad's fine battery or its unplugging takes the line back, not another pad's.
int g_batteryNews = -1;
SDL_JoystickID g_batteryNewsPad = 0;
GlyphChoice g_glyphChoice = GlyphChoice::Auto;
// The player's button layout (SetLayout): the pads keep their buttons by place, the shared state is
// the game's buttons through it.
Layout g_layout = Layout::Standard;
// The game's window has the focus (SetFocused): without it the pads' presses and pushes go unread.
bool g_focused = true;

// The pad's kind: SDL's own guess from its vendor and product where it has one (a PC), else its name
// (the browser gives SDL only the Gamepad.id).
PadKind KindOf(SDL_Gamepad* handle)
{
    switch (SDL_GetGamepadType(handle))
    {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return PadKind::PlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return PadKind::Nintendo;
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return PadKind::Xbox;
    default:
        break;
    }
    const char* name = SDL_GetGamepadName(handle);
    return KindFromName(name != nullptr ? name : "");
}

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

#ifndef __EMSCRIPTEN__
// The pad's battery as SDL tells it, as the pad opens and as it changes: a low one is news, a fine one
// again takes back its own low line not said yet (another pad's stays).
void NoteBattery(Pad& pad, SDL_PowerState state, int percent)
{
    const bool onBattery = state == SDL_POWERSTATE_ON_BATTERY;
    if (const std::optional<int> low = pad.battery.Note(percent, onBattery))
    {
        g_batteryNews = *low;
        g_batteryNewsPad = pad.id;
    }
    else if (BatteryWatch::IsFine(percent, onBattery) && pad.id == g_batteryNewsPad)
        g_batteryNews = -1;
}
#endif

bool AnyPadHolds(int button)
{
    return std::any_of(g_pads.begin(), g_pads.end(),
                       [button](const Pad& pad) { return pad.handle != nullptr && pad.held[button]; });
}

// One pad's button changed: the shared state of the game's button it plays (the layout's) follows,
// a press counts when no other pad held it.
void SetPadButton(Pad& pad, Button button, bool down)
{
    const auto index = static_cast<int>(button);
    if (pad.held[index] == down)
        return;
    ButtonState& played = g_buttons[static_cast<int>(Swapped(button, g_layout))];
    const bool wasHeld = played.held;
    pad.held[index] = down;
    played.held = AnyPadHolds(index);
    if (!wasHeld && played.held)
        ++played.presses;
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

// A press and its release at once, of a game's button no pad holds: the reader sees one tap.
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
            Tap(Swapped(StandardButtons[index], g_layout));
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
    free->kind = KindOf(handle);
    g_lastKind = free->kind;
    const char* name = SDL_GetGamepadName(handle);
    mu::log::Get("input")->info("Gamepad connected: {} ({})", name != nullptr ? name : "unnamed",
                                free->kind == PadKind::PlayStation ? "PlayStation"
                                : free->kind == PadKind::Nintendo  ? "Nintendo"
                                                                   : "Xbox");
#ifdef __EMSCRIPTEN__
    // A pad woken in a background tab (its press meant for another page) taps nothing here.
    if (g_focused)
        TapWakingPress();
#else
    int percent = -1;
    const SDL_PowerState power = SDL_GetGamepadPowerInfo(handle, &percent);
    NoteBattery(*free, power, percent);
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

// A pad button is held or a stick pushed as far as takes the game from the mouse (a worn stick
// drifting near its centre does not count): the thumbs are on the pad.
bool IsPadBusy()
{
    const bool buttonHeld =
        std::any_of(g_buttons.begin(), g_buttons.end(), [](const ButtonState& state) { return state.held; });
    const bool stickPushed = std::any_of(g_pads.begin(), g_pads.end(), [](const Pad& pad) {
        return pad.handle != nullptr && (RawLean(pad, SDL_GAMEPAD_AXIS_LEFTX) > TakeOverLean ||
                                         RawLean(pad, SDL_GAMEPAD_AXIS_RIGHTX) > TakeOverLean);
    });
    return buttonHeld || stickPushed;
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
    g_offWhenPutDown = false;
    if (!enabled && g_enabled && g_usedLast && IsConnected())
    {
        g_offWhenPutDown = true;
        mu::log::Get("input")->info("Gamepad off once another device is pressed");
        return;
    }
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
        // With the pads closed no pad's fine battery or unplugging takes a low battery's line back: the
        // line goes with them, else it would show on the next pad turned on (pad round 8, D1).
        g_batteryNews = -1;
        g_batteryNewsPad = 0;
    }
    mu::log::Get("input")->info("Gamepad {}", enabled ? "on" : "off");
}

bool IsEnabled()
{
    return g_enabled && !g_offWhenPutDown;
}

bool TurnsOffWhenPutDown()
{
    return g_offWhenPutDown;
}

void SetRumble(bool on)
{
    g_rumble = on;
    if (on)
        return;
    for (const Pad& pad : g_pads)
    {
        if (pad.handle != nullptr)
            SDL_RumbleGamepad(pad.handle, 0, 0, 0);
    }
}

bool IsRumbleOn()
{
    return g_rumble;
}

void SetLayout(Layout layout)
{
    if (layout == g_layout)
        return;
    // Let go under the old layout, so each game's button held is released, not one it no longer plays.
    Reset();
    g_layout = layout;
    mu::log::Get("input")->info("Gamepad layout {}", static_cast<int>(layout));
}

Layout GetLayout()
{
    return g_layout;
}

PadKind ConnectedKind()
{
    return IsConnected() ? g_lastKind : PadKind::Xbox;
}

void SetGlyphChoice(GlyphChoice choice)
{
    g_glyphChoice = choice;
}

GlyphChoice GetGlyphChoice()
{
    return g_glyphChoice;
}

PadKind ShownKind()
{
    return Core::Input::Gamepad::ShownKind(g_glyphChoice, ConnectedKind());
}

void Rumble(float low, float high, int milliseconds)
{
    if (!g_rumble || !IsEnabled() || milliseconds <= 0)
        return;
    // SDL replaces the shake under way: a weaker one would cut a stronger one short.
    static RumbleArbiter s_arbiter;
    const RumbleArbiter::Clock::time_point now = RumbleArbiter::Clock::now();
    if (!s_arbiter.Allows(low, high, milliseconds, now))
        return;
    const auto strength = [](float share) {
        return static_cast<Uint16>(std::clamp(share, 0.f, 1.f) * 65535.f);
    };
    bool played = false;
    for (const Pad& pad : g_pads)
    {
        if (pad.handle != nullptr &&
            SDL_RumbleGamepad(pad.handle, strength(low), strength(high), static_cast<Uint32>(milliseconds)))
            played = true;
    }
    // A pad that has no motors (or refused) shakes nothing: no shake is under way to keep.
    if (played)
        s_arbiter.Record(low, high, milliseconds, now);
}

void ProcessEvent(const SDL_Event& event)
{
    if (IsOtherDevicePress(event))
    {
        // A handheld's extra buttons (its back paddles, its own keys) send keys while the thumbs stay
        // on the pad: such a key does not take the game from a pad that holds a button or a pushed
        // stick. The key itself still reaches the game. A mouse button or a finger always takes it.
        if (event.type == SDL_EVENT_KEY_DOWN && IsPadBusy())
            return;
        g_usedLast = false;
        if (g_offWhenPutDown)
            SetEnabled(false);
        return;
    }
    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
    {
        const bool known = FindPad(event.gdevice.which) != nullptr;
        AddPad(event.gdevice.which);
        if (!known && FindPad(event.gdevice.which) != nullptr)
            g_news = PadNews::Connected;
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
    {
        const bool known = FindPad(event.gdevice.which) != nullptr;
        RemovePad(event.gdevice.which);
        // A low line not said yet goes with its pad.
        if (known && event.gdevice.which == g_batteryNewsPad)
            g_batteryNews = -1;
        // One pad of two pulled out leaves one playing: no news.
        if (known && !IsConnected())
        {
            g_news = PadNews::Disconnected;
            g_batteryNews = -1;
        }
        // The last pad unplugged: nothing is left to play, so the off it waited for comes now.
        if (g_offWhenPutDown && !IsConnected())
            SetEnabled(false);
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        // Without the focus a press is meant for another window; a release still lets go.
        if (event.gbutton.down && !g_focused)
            break;
        if (Pad* pad = FindPad(event.gbutton.which))
        {
            if (event.gbutton.down)
                g_lastKind = pad->kind;
            if (const std::optional<Button> button = FromSdlButton(event.gbutton.button))
                SetPadButton(*pad, *button, event.gbutton.down);
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (!g_focused)
            break;
        if (Pad* pad = FindPad(event.gaxis.which))
            SetAxis(*pad, event.gaxis.axis, AxisPosition(event.gaxis.value));
        break;
#ifndef __EMSCRIPTEN__
    case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
        if (Pad* pad = FindPad(event.jbattery.which))
            NoteBattery(*pad, event.jbattery.state, event.jbattery.percent);
        break;
#endif
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
            // Without the focus the stick is only followed: back in focus, a stick still held where it
            // was stays let go until it moves again.
            if (g_focused)
                SetAxis(pad, axis, AxisPosition(value));
        }
    }
}

PadNews TakeNews()
{
    return std::exchange(g_news, PadNews::None);
}

int TakeBatteryNews()
{
    return std::exchange(g_batteryNews, -1);
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

StickPosition PhysicalLeftStick()
{
    return Stick(SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY);
}

StickPosition PhysicalRightStick()
{
    return Stick(SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY);
}
} // namespace

StickPosition LeftStick()
{
    return SwapsSticks(g_layout) ? PhysicalRightStick() : PhysicalLeftStick();
}

StickPosition RightStick()
{
    return SwapsSticks(g_layout) ? PhysicalLeftStick() : PhysicalRightStick();
}

void Reset()
{
    for (Pad& pad : g_pads)
    {
        if (pad.handle != nullptr)
            ReleasePad(pad);
    }
}

void SetFocused(bool focused)
{
    if (!focused && g_focused)
        Reset();
    g_focused = focused;
}
} // namespace Core::Input::Gamepad
