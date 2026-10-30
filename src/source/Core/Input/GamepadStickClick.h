// A stick's own button (LS, RS: the stick pushed down) pressed on purpose. A thumb pushing the stick
// hard to its rim clicks it now and then without meaning to, so a click counts only when the stick
// stood away from its rim as it went down and a moment before. A frame can be long on a slow phone: a
// thumb that swung the stick out and let it spring back within one frame still swung it. A press
// made with the stick back from its rim that is still held once the stick stood there a moment
// counts then: a click right after aiming the pointer.
#pragma once

#include <chrono>

namespace Core::Input::Gamepad
{
class StickClick
{
public:
    using Clock = std::chrono::steady_clock;

    // Once a frame: the stick's lean past its dead zone (0 .. 1), whether its button was pressed since
    // the last frame, and whether it is held now.
    void Update(float lean, bool pressed, bool held, Clock::time_point now);
    // The button went down on purpose since the last frame (it may be up again already).
    [[nodiscard]] bool Pressed() const
    {
        return m_Pressed;
    }
    // A press on purpose, still held.
    [[nodiscard]] bool Held() const
    {
        return m_Accepted && m_HeldNow;
    }

private:
    bool m_Swung = false;
    Clock::time_point m_LastSwing{};
    // Pressed too soon after a swing, with the stick back from its rim: counts if still held later.
    bool m_Waiting = false;
    bool m_Accepted = false;
    bool m_Pressed = false;
    bool m_HeldNow = false;
};
} // namespace Core::Input::Gamepad
