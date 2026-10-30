// Web build only: see WebGamepad.h.

#include "Core/Platform/web/WebGamepad.h"

#include <emscripten/html5.h>

#include <algorithm>
#include <cstring>

namespace
{
// The buttons of the "standard" layout the game reads; the guide button (16) is the system's.
constexpr int StandardButtonCount = 16;
constexpr int LeftTriggerButton = 6;
constexpr int RightTriggerButton = 7;

bool IsStandard(const EmscriptenGamepadEvent& pad)
{
    return pad.connected && std::strcmp(pad.mapping, "standard") == 0;
}

bool IsDown(const EmscriptenGamepadEvent& pad, int button, float triggerPull)
{
    if (button == LeftTriggerButton || button == RightTriggerButton)
        return pad.analogButton[button] > triggerPull;
    return pad.digitalButton[button] != 0;
}
} // namespace

std::uint32_t mu::web::HeldStandardButtons(float triggerPull)
{
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
        return 0;
    std::uint32_t held = 0;
    const int pads = emscripten_get_num_gamepads();
    for (int index = 0; index < pads; ++index)
    {
        EmscriptenGamepadEvent pad{};
        if (emscripten_get_gamepad_status(index, &pad) != EMSCRIPTEN_RESULT_SUCCESS || !IsStandard(pad))
            continue;
        const int buttons = std::min(pad.numButtons, StandardButtonCount);
        for (int button = 0; button < buttons; ++button)
        {
            if (IsDown(pad, button, triggerPull))
                held |= 1u << button;
        }
    }
    return held;
}
