// The page a pad button acts on (the right trigger held turns X, Y, B and the left bumper and
// trigger to their second controls, the right bumper held turns X, Y, B to their third), taken as
// the button goes down and kept until it is let go. Read every frame instead, letting the trigger go
// before X turned skill 4 into skill 1, and the game took the change as a fresh press of skill 1: the
// wrong skill was cast, the wrong potion drunk.
#pragma once

#include <cstdint>

namespace Core::Input::Gamepad
{
class PageLatch
{
public:
    // Once a frame: whether the button is held, its press counter (ButtonState::presses: a press
    // and release between two frames still moves it), and the page up now (0 the first).
    // Returns the page the button acts on this frame.
    [[nodiscard]] int Page(bool held, std::uint32_t presses, int pageNow);

private:
    bool m_Latched = false;
    int m_Page = 0;
    std::uint32_t m_Presses = 0;
};
} // namespace Core::Input::Gamepad
