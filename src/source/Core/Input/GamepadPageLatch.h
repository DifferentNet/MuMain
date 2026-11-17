// The page a pad button acts on (the right trigger held turns X, Y, B and the left bumper and
// trigger to their second controls), taken as the button goes down and kept until it is let go.
// Read every frame instead, letting the trigger go before X turned skill 4 into skill 1, and the
// game took the change as a fresh press of skill 1: the wrong skill was cast, the wrong potion drunk.
#pragma once

#include <cstdint>

namespace Core::Input::Gamepad
{
class PageLatch
{
public:
    // Once a frame: whether the button is held, its press counter (ButtonState::presses: a press
    // and release between two frames still moves it), and whether the second page is up now.
    // Returns whether the button acts on the second page this frame.
    [[nodiscard]] bool Alt(bool held, std::uint32_t presses, bool altNow);

private:
    bool m_Latched = false;
    bool m_Alt = false;
    std::uint32_t m_Presses = 0;
};
} // namespace Core::Input::Gamepad
