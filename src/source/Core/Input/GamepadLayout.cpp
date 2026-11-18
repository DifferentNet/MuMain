#include "Core/Input/GamepadLayout.h"

namespace Core::Input::Gamepad
{
Layout LayoutFromIndex(int index)
{
    if (index < 0 || index >= LayoutCount)
        return Layout::Standard;
    return static_cast<Layout>(index);
}

bool SwapsSticks(Layout layout)
{
    return layout == Layout::LeftHanded;
}

bool SwapsShoulders(Layout layout)
{
    return layout == Layout::SwappedShoulders;
}

Button Swapped(Button button, Layout layout)
{
    if (SwapsSticks(layout))
    {
        if (button == Button::LeftStick)
            return Button::RightStick;
        if (button == Button::RightStick)
            return Button::LeftStick;
    }
    if (SwapsShoulders(layout))
    {
        switch (button)
        {
        case Button::LeftShoulder:
            return Button::LeftTrigger;
        case Button::LeftTrigger:
            return Button::LeftShoulder;
        case Button::RightShoulder:
            return Button::RightTrigger;
        case Button::RightTrigger:
            return Button::RightShoulder;
        default:
            break;
        }
    }
    return button;
}
} // namespace Core::Input::Gamepad
