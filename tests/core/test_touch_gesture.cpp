#include <doctest.h>

#include "Core/Input/TouchGesture.h"

#include <chrono>

namespace Gesture = Core::Input::Touch::Gesture;

namespace
{
using Gesture::Landing;
using Gesture::Lift;
using Gesture::Phase;
using std::chrono::milliseconds;

constexpr milliseconds JustBefore = Gesture::LongPressTime - milliseconds(1);
} // namespace

TEST_CASE("a first finger starts a gesture, a second next to one that has not moved pinches [core][touch]")
{
    CHECK(Gesture::OnLanding(Phase::None) == Landing::Start);
    CHECK(Gesture::OnLanding(Phase::Pending) == Landing::Pinch);
}

TEST_CASE("a second finger calls a drag off and pinches, nothing is dropped [core][touch]")
{
    CHECK(Gesture::OnLanding(Phase::Dragging) == Landing::CancelDragThenPinch);
}

TEST_CASE("a finger landing while one of a pinch's fingers is still down goes on pinching [core][touch]")
{
    CHECK(Gesture::OnLanding(Phase::Ending) == Landing::ResumePinch);
}

TEST_CASE("a finger next to a scroll, a long press or a pinch under way does nothing [core][touch]")
{
    CHECK(Gesture::OnLanding(Phase::Scrolling) == Landing::Ignore);
    CHECK(Gesture::OnLanding(Phase::LongPress) == Landing::Ignore);
    CHECK(Gesture::OnLanding(Phase::Pinch) == Landing::Ignore);
}

TEST_CASE("a tap clicks, unless it stopped a glide or the system took it [core][touch]")
{
    CHECK(Gesture::OnLift(Phase::Pending, false, false) == Lift::Click);
    CHECK(Gesture::OnLift(Phase::Pending, false, true) == Lift::Nothing);
    CHECK(Gesture::OnLift(Phase::Pending, true, false) == Lift::Nothing);
}

TEST_CASE("a drag drops where the finger lifts, a cancelled drag drops nothing [core][touch]")
{
    CHECK(Gesture::OnLift(Phase::Dragging, false, false) == Lift::Drop);
    CHECK(Gesture::OnLift(Phase::Dragging, true, false) == Lift::CancelDrag);
}

TEST_CASE("lifting ends a scroll, a long press and a pinch [core][touch]")
{
    CHECK(Gesture::OnLift(Phase::Scrolling, false, false) == Lift::EndScroll);
    CHECK(Gesture::OnLift(Phase::Scrolling, true, false) == Lift::EndScroll);
    CHECK(Gesture::OnLift(Phase::LongPress, false, false) == Lift::ReleaseRight);
    CHECK(Gesture::OnLift(Phase::LongPress, true, false) == Lift::ReleaseRight);
    CHECK(Gesture::OnLift(Phase::Pinch, false, false) == Lift::EndPinch);
}

TEST_CASE("the last finger of a pinch, or none at all, does nothing when it lifts [core][touch]")
{
    CHECK(Gesture::OnLift(Phase::Ending, false, false) == Lift::Nothing);
    CHECK(Gesture::OnLift(Phase::None, false, false) == Lift::Nothing);
}

TEST_CASE("a finger held still becomes a long press after the long press time [core][touch]")
{
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::Pending, JustBefore, false));
    CHECK(Gesture::IsLongPressDue(Phase::Pending, Gesture::LongPressTime, false));
}

TEST_CASE("no long press while the mouse carries an item, so the finger can still drag it [core][touch]")
{
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::Pending, Gesture::LongPressTime, true));
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::Pending, milliseconds(5000), true));
}

TEST_CASE("only a finger that has not moved yet becomes a long press [core][touch]")
{
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::Dragging, milliseconds(5000), false));
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::Scrolling, milliseconds(5000), false));
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::LongPress, milliseconds(5000), false));
    CHECK_FALSE(Gesture::IsLongPressDue(Phase::None, milliseconds(5000), false));
}
