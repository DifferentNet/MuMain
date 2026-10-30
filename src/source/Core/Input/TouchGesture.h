// The rules of the finger that plays the mouse (TouchPointer.cpp), apart from SDL so they can be
// tested: what a finger landing or lifting does in each phase of the gesture, and when a finger held
// still becomes a long press.
#pragma once

#include <chrono>
#include <cstdint>

namespace Core::Input::Touch::Gesture
{
// A finger held still this long clicks the right button (as Remote Desktop and Android's mouse
// emulation do), and holds it until the finger lifts.
inline constexpr auto LongPressTime = std::chrono::milliseconds(500);

enum class Phase : std::uint8_t
{
    // No finger plays the mouse.
    None,
    // Down, not yet a tap, a drag or a long press.
    Pending,
    // Moved: the left button is held and follows the finger.
    Dragging,
    // Moved over a scrollable list of the UI: the list follows the finger, no button is held.
    Scrolling,
    // Held still: the right button is held.
    LongPress,
    // Two fingers: the wheel turns as they spread or close.
    Pinch,
    // One finger of a pinch has lifted, the other is still down.
    Ending,
};

// What a finger landing on no control does.
enum class Landing : std::uint8_t
{
    // The first finger: a tap, a drag or a long press to come.
    Start,
    // A second finger next to a first that has not moved yet: the pair pinches.
    Pinch,
    // A second finger while the first drags: the first had only begun the pinch, so its drag is
    // called off, nothing dropped, and the pair pinches.
    CancelDragThenPinch,
    // A finger while one of a pinch's fingers is still down: the pinch goes on with the two.
    ResumePinch,
    // A finger next to a scrolling list, a long press or a pinch already under way: nothing.
    Ignore,
};

[[nodiscard]] Landing OnLanding(Phase phase);

// What lifting the finger that plays the mouse does.
enum class Lift : std::uint8_t
{
    // A tap: the left button clicks where the finger lifted.
    Click,
    // A tap that only stopped a gliding list, a tap the system took away, or the end of a pinch's
    // last finger: no click.
    Nothing,
    // The list stops following, or glides on after a flick.
    EndScroll,
    // A drag ends where the finger lifted: the left button is let go there, as a drop.
    Drop,
    // The system took a dragging finger away (a gesture of the browser's): the left button is let
    // go off the window, so nothing takes it for a drop.
    CancelDrag,
    // A long press ends: the right button is let go.
    ReleaseRight,
    // One finger of a pinch lifts: no more wheel until both are up.
    EndPinch,
};

[[nodiscard]] Lift OnLift(Phase phase, bool cancelled, bool stoppedGlide);

// Whether a finger held still `held` long has become a long press. Not while the mouse carries an
// item: the right button would put it back in the bag, and the finger could no longer drag it.
[[nodiscard]] bool IsLongPressDue(Phase phase, std::chrono::milliseconds held, bool carryingItem);

// Whether a finger's move by (dx, dy) goes more up or down than sideways: a slider in a list that
// scrolls up and down takes only a sideways drag, the list the rest. A move as much sideways as up
// or down is the slider's.
[[nodiscard]] bool IsMostlyVertical(float dx, float dy);
} // namespace Core::Input::Touch::Gesture
