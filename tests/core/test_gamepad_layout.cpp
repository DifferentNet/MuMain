#include <doctest.h>

#include "Core/Input/GamepadLayout.h"

namespace Pad = Core::Input::Gamepad;
using Pad::Button;
using Pad::Layout;

namespace
{
constexpr Layout AllLayouts[] = {Layout::Standard, Layout::LeftHanded, Layout::SwappedShoulders};
constexpr Button FixedButtons[] = {Button::A,      Button::B,      Button::X,        Button::Y,
                                   Button::View,   Button::Menu,   Button::DpadUp,   Button::DpadDown,
                                   Button::DpadLeft, Button::DpadRight};
} // namespace

TEST_CASE("each layout is its own inverse [core][gamepad]")
{
    for (const Layout layout : AllLayouts)
    {
        for (int index = 0; index < Pad::ButtonCount; ++index)
        {
            const auto button = static_cast<Button>(index);
            CHECK(Pad::Swapped(Pad::Swapped(button, layout), layout) == button);
        }
    }
}

TEST_CASE("the face buttons, View, Menu and the d-pad never move [core][gamepad]")
{
    for (const Layout layout : AllLayouts)
    {
        for (const Button button : FixedButtons)
            CHECK(Pad::Swapped(button, layout) == button);
    }
}

TEST_CASE("the standard layout moves nothing [core][gamepad]")
{
    for (int index = 0; index < Pad::ButtonCount; ++index)
    {
        const auto button = static_cast<Button>(index);
        CHECK(Pad::Swapped(button, Layout::Standard) == button);
    }
    CHECK_FALSE(Pad::SwapsSticks(Layout::Standard));
    CHECK_FALSE(Pad::SwapsShoulders(Layout::Standard));
}

TEST_CASE("left-handed trades the sticks and their clicks only [core][gamepad]")
{
    CHECK(Pad::SwapsSticks(Layout::LeftHanded));
    CHECK_FALSE(Pad::SwapsShoulders(Layout::LeftHanded));
    CHECK(Pad::Swapped(Button::LeftStick, Layout::LeftHanded) == Button::RightStick);
    CHECK(Pad::Swapped(Button::RightStick, Layout::LeftHanded) == Button::LeftStick);
    CHECK(Pad::Swapped(Button::LeftShoulder, Layout::LeftHanded) == Button::LeftShoulder);
    CHECK(Pad::Swapped(Button::RightTrigger, Layout::LeftHanded) == Button::RightTrigger);
}

TEST_CASE("swapped shoulders trades each bumper with the trigger under it [core][gamepad]")
{
    CHECK(Pad::SwapsShoulders(Layout::SwappedShoulders));
    CHECK_FALSE(Pad::SwapsSticks(Layout::SwappedShoulders));
    CHECK(Pad::Swapped(Button::LeftShoulder, Layout::SwappedShoulders) == Button::LeftTrigger);
    CHECK(Pad::Swapped(Button::LeftTrigger, Layout::SwappedShoulders) == Button::LeftShoulder);
    CHECK(Pad::Swapped(Button::RightShoulder, Layout::SwappedShoulders) == Button::RightTrigger);
    CHECK(Pad::Swapped(Button::RightTrigger, Layout::SwappedShoulders) == Button::RightShoulder);
    CHECK(Pad::Swapped(Button::LeftStick, Layout::SwappedShoulders) == Button::LeftStick);
}

TEST_CASE("a saved number out of range is the standard layout [core][gamepad]")
{
    CHECK(Pad::LayoutFromIndex(0) == Layout::Standard);
    CHECK(Pad::LayoutFromIndex(1) == Layout::LeftHanded);
    CHECK(Pad::LayoutFromIndex(2) == Layout::SwappedShoulders);
    CHECK(Pad::LayoutFromIndex(3) == Layout::Standard);
    CHECK(Pad::LayoutFromIndex(-1) == Layout::Standard);
}
