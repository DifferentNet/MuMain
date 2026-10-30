#include "stdafx.h"
#include "UI/Mobile/ClickSource.h"

#include "Core/Input/GamepadInput.h"
#include "Core/Input/GamepadPointer.h"
#include "Core/Input/TouchInput.h"
#include "Core/Input/TouchPointer.h"

#include <SDL3/SDL_mouse.h>

namespace UI::Mobile::ClickSource
{
namespace Finger = Core::Input::Touch::Pointer;

bool IsFingerTap()
{
    return Finger::LastLeftPress() == Finger::LeftPress::Tap;
}

bool IsFingerDrag()
{
    return Finger::LastLeftPress() == Finger::LeftPress::Drag;
}

bool IsFingerLongPress()
{
    return Core::Input::Touch::IsMouseButtonHeld(SDL_BUTTON_RIGHT);
}

bool IsPadRightClick()
{
    return Core::Input::Gamepad::WasUsedLast() && Core::Input::Gamepad::Pointer::IsButtonHeld(SDL_BUTTON_RIGHT);
}
} // namespace UI::Mobile::ClickSource
