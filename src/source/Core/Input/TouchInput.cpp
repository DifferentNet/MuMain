#include "stdafx.h"
#include "Core/Input/TouchInput.h"
#include "Core/Input/TouchPointer.h"

#include <SDL3/SDL.h>

#include <array>

namespace Core::Input::Touch
{
namespace
{
// Browsers report up to ten fingers; more are ignored.
constexpr int MaxFingers = 10;
// What a finger is doing besides holding a control (whose id is >= 0).
constexpr int PointerOwner = -2;
constexpr int IgnoredOwner = -3;
// A tapped key stays held while this counter is above zero; BeginFrame lowers it once a frame before
// the key-state scan, so the scan sees it held exactly once whether the tap came before or after it.
constexpr std::uint8_t TapFrames = 2;
constexpr int KeyCount = 256;

struct FingerSlot
{
    bool used = false;
    SDL_FingerID id = 0;
    int owner = IgnoredOwner;
};

bool g_enabled = false;
HitTest g_hitTest = nullptr;
SDL_Window* g_window = nullptr;
std::array<FingerSlot, MaxFingers> g_fingers{};
std::array<ControlFinger, MaxControls> g_controls{};
std::array<bool, KeyCount> g_heldKeys{};
std::array<std::uint8_t, KeyCount> g_tappedKeys{};
const ControlFinger g_noControl{};

FingerSlot* FindFinger(SDL_FingerID id)
{
    for (FingerSlot& slot : g_fingers)
    {
        if (slot.used && slot.id == id)
            return &slot;
    }
    return nullptr;
}

FingerSlot* FreeFinger()
{
    for (FingerSlot& slot : g_fingers)
    {
        if (!slot.used)
            return &slot;
    }
    return nullptr;
}

bool IsControl(int owner)
{
    return owner >= 0 && owner < MaxControls;
}

void PressControl(int controlId, float x, float y)
{
    ControlFinger& control = g_controls[controlId];
    control.held = true;
    ++control.presses;
    control.downX = control.x = x;
    control.downY = control.y = y;
}

void ReleaseControl(int controlId, float x, float y)
{
    ControlFinger& control = g_controls[controlId];
    control.x = x;
    control.y = y;
    if (!control.held)
        return;
    control.held = false;
    ++control.releases;
}

void FingerDown(SDL_FingerID id, float x, float y, SDL_Window* window)
{
    FingerSlot* slot = FindFinger(id);
    if (slot == nullptr)
        slot = FreeFinger();
    if (slot == nullptr)
        return;

    slot->used = true;
    slot->id = id;
    const int control = g_hitTest != nullptr ? g_hitTest(x, y) : NoControl;
    // One finger per control: a second finger landing on a held control (a thumb sliding from the
    // joystick onto it) does nothing.
    if (IsControl(control) && !g_controls[control].held)
    {
        slot->owner = control;
        PressControl(control, x, y);
        return;
    }
    if (IsControl(control))
    {
        slot->owner = IgnoredOwner;
        return;
    }
    slot->owner = PointerOwner;
    Pointer::Down(id, x, y, window);
}

void FingerMove(SDL_FingerID id, float x, float y, SDL_Window* window)
{
    FingerSlot* slot = FindFinger(id);
    if (slot == nullptr)
        return;
    if (IsControl(slot->owner))
    {
        g_controls[slot->owner].x = x;
        g_controls[slot->owner].y = y;
        return;
    }
    if (slot->owner == PointerOwner)
        Pointer::Move(id, x, y, window);
}

void FingerUp(SDL_FingerID id, float x, float y, bool cancelled, SDL_Window* window)
{
    FingerSlot* slot = FindFinger(id);
    if (slot == nullptr)
        return;
    if (IsControl(slot->owner))
        ReleaseControl(slot->owner, x, y);
    else if (slot->owner == PointerOwner)
        Pointer::Up(id, x, y, cancelled, window);
    *slot = FingerSlot{};
}

bool IsKeyIndex(int virtualKey)
{
    return virtualKey >= 0 && virtualKey < KeyCount;
}
} // namespace

void SetEnabled(bool enabled)
{
    if (enabled == g_enabled)
        return;
    if (!enabled)
        Reset();
    g_enabled = enabled;
    // On, SDL's own emulation (the first finger anywhere is the mouse) would turn the joystick's
    // finger into a click on the ground; fingers are routed here instead.
    if (enabled)
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    else
        SDL_ResetHint(SDL_HINT_TOUCH_MOUSE_EVENTS);
}

bool IsEnabled()
{
    return g_enabled;
}

void SetHitTest(HitTest hitTest)
{
    g_hitTest = hitTest;
}

bool ProcessEvent(const SDL_Event& event, SDL_Window* window)
{
    switch (event.type)
    {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        break;
    default:
        return false;
    }
    // Touch events SDL makes up from a real mouse (SDL_HINT_MOUSE_TOUCH_EVENTS) are the mouse's own.
    if (!g_enabled || window == nullptr || event.tfinger.touchID == SDL_MOUSE_TOUCHID)
        return true;

    g_window = window;
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    // Window coordinates, the units of SDL's mouse events (what SDL's own emulation uses too).
    const float x = event.tfinger.x * static_cast<float>(width);
    const float y = event.tfinger.y * static_cast<float>(height);
    const SDL_FingerID id = event.tfinger.fingerID;

    switch (event.type)
    {
    case SDL_EVENT_FINGER_DOWN:
        FingerDown(id, x, y, window);
        break;
    case SDL_EVENT_FINGER_MOTION:
        FingerMove(id, x, y, window);
        break;
    case SDL_EVENT_FINGER_UP:
        FingerUp(id, x, y, false, window);
        break;
    default:
        FingerUp(id, x, y, true, window);
        break;
    }
    return true;
}

void BeginFrame(SDL_Window* window)
{
    for (std::uint8_t& frames : g_tappedKeys)
    {
        if (frames > 0)
            --frames;
    }
    if (g_enabled && window != nullptr)
        Pointer::Tick(window);
}

void Reset()
{
    Pointer::Reset(g_window);
    for (int control = 0; control < MaxControls; ++control)
        ReleaseControl(control, g_controls[control].x, g_controls[control].y);
    g_fingers.fill(FingerSlot{});
    g_heldKeys.fill(false);
    g_tappedKeys.fill(0);
}

void RestPointer()
{
    if (g_enabled)
        Pointer::Rest(g_window);
}

const ControlFinger& Control(int controlId)
{
    return IsControl(controlId) ? g_controls[controlId] : g_noControl;
}

void SetKeyHeld(int virtualKey, bool held)
{
    if (IsKeyIndex(virtualKey))
        g_heldKeys[virtualKey] = held;
}

void TapKey(int virtualKey)
{
    if (IsKeyIndex(virtualKey))
        g_tappedKeys[virtualKey] = TapFrames;
}

bool IsKeyHeld(int virtualKey)
{
    return IsKeyIndex(virtualKey) && (g_heldKeys[virtualKey] || g_tappedKeys[virtualKey] > 0);
}

bool IsMouseButtonHeld(int sdlButton)
{
    return Pointer::IsButtonHeld(sdlButton);
}
} // namespace Core::Input::Touch
