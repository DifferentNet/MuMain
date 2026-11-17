// The right stick turning the mouse wheel over a window's long text (pad round 5, 2026-11-17).
//
// With the frame up the d-pad hops between a window's targets and scrolls a list only as the frame
// passes its end, so a quest's text, the help or a letter with one button below could not be read to
// its end. Now the right stick pushed hard up or down over something that scrolls turns the wheel
// there, quicker the harder it is pushed and by the clock, so the pace is the same at 30 and 144
// frames a second. A softer push, or one more to the side, still moves the pointer as before.
#pragma once

#include "Core/Input/GamepadInput.h"

namespace Core::Input::Gamepad
{
class StickWheel
{
public:
    // The push up or down (past the dead zone) that starts the wheel, and the one under which it stops:
    // a stick held near the start does not start and stop in turn.
    static constexpr float StartLean = 0.6f;
    static constexpr float StopLean = 0.5f;
    // Notches a second at the start push and at the rim: about the d-pad wheel's pace in between.
    static constexpr float SlowestPace = 6.f;
    static constexpr float FastestPace = 22.f;
    // A long frame (a hitch, a window dragged) turns at most this many notches at once.
    static constexpr int MostSteps = 3;

    // Whether the stick leans so that it would turn the wheel (or goes on turning it): the cheap test,
    // before the pointer's place is looked up.
    [[nodiscard]] bool MayTurn(StickPosition stick) const;

    // Once a frame: the notches to turn now (positive up, as Pointer::TurnWheel takes them), given
    // whether the pointer is over something that scrolls. The first notch turns at once, then they go
    // by `seconds`, the frame's length.
    [[nodiscard]] int Take(StickPosition stick, bool overScroll, float seconds);

    // Whether the stick works the wheel now (as of the last Take): the pointer stays where it is meanwhile.
    [[nodiscard]] bool Turning() const { return m_Turning; }

    void Reset();

private:
    bool m_Turning = false;
    // The notches owed by the time passed, short of a whole one.
    float m_Owed = 0.f;
};
} // namespace Core::Input::Gamepad
