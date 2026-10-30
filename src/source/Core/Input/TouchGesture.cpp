#include "Core/Input/TouchGesture.h"

namespace Core::Input::Touch::Gesture
{
Landing OnLanding(Phase phase)
{
    switch (phase)
    {
    case Phase::None:
        return Landing::Start;
    case Phase::Pending:
        return Landing::Pinch;
    case Phase::Dragging:
        return Landing::CancelDragThenPinch;
    case Phase::Ending:
        return Landing::ResumePinch;
    default:
        return Landing::Ignore;
    }
}

Lift OnLift(Phase phase, bool cancelled, bool stoppedGlide)
{
    switch (phase)
    {
    case Phase::Pending:
        return cancelled || stoppedGlide ? Lift::Nothing : Lift::Click;
    case Phase::Scrolling:
        return Lift::EndScroll;
    case Phase::Dragging:
        return cancelled ? Lift::CancelDrag : Lift::Drop;
    case Phase::LongPress:
        return Lift::ReleaseRight;
    case Phase::Pinch:
        return Lift::EndPinch;
    default:
        return Lift::Nothing;
    }
}

bool IsLongPressDue(Phase phase, std::chrono::milliseconds held, bool carryingItem)
{
    return phase == Phase::Pending && held >= LongPressTime && !carryingItem;
}
} // namespace Core::Input::Touch::Gesture
